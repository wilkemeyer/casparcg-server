#include "../../StdAfx.h"

#include "frame_processor.h"

#include "../../../common/gpu/pixel_buffer.h"

#include "frame.h"
#include "../frame.h"
#include "../format.h"
#include "../algorithm.h"
#include "../system_frame.h"

#include "../../../common/exception/exceptions.h"

#include <Glee.h>
#include <SFML/Window.hpp>

#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread.hpp>
#include <boost/range.hpp>
#include <boost/foreach.hpp>

#include <functional>
#include <unordered_map>
#include <numeric>

namespace caspar { namespace gpu {
	
class frame_buffer
{
public:
	frame_buffer(size_t width, size_t height) : texture_(width, height)
	{
		glGenFramebuffersEXT(1, &fbo_);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
		glBindTexture(GL_TEXTURE_2D, texture_.handle());
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture_.handle(), 0);
	}

	~frame_buffer()
	{
		glDeleteFramebuffersEXT(1, &fbo_);
	}
		
	GLuint handle() { return fbo_; }
	GLenum attachement() { return GL_COLOR_ATTACHMENT0_EXT; }
	
private:
	common::gpu::texture texture_;
	GLuint fbo_;
};
typedef std::shared_ptr<frame_buffer> frame_buffer_ptr;

struct frame_processor::implementation
{	
	implementation(const frame_format_desc& format_desc) : format_desc_(format_desc)
	{
		execution_queue_.set_capacity(6);
		thread_ = boost::thread([=]{run();});
		empty_frame_ = clear_frame(std::make_shared<system_frame>(format_desc));
	}

	~implementation()
	{
		execution_queue_.push(nullptr);
		finished_frames_.push(nullptr);
		thread_.join();
	}
		
	void composite(const std::vector<frame_ptr>& frames)
	{
		if(frames.empty())
			finished_frames_.push(empty_frame_);
		else
			execution_queue_.push([=]{do_composite(frames);});
	}

	void do_composite(const std::vector<frame_ptr>& frames)
	{			
		index_ = (index_ + 1) % 2;
		int next_index = (index_ + 1) % 2;

		// 2. Start asynchronous DMA transfer to video memory

		// Lock frames and give pointer ownership to OpenGL
		boost::range::for_each(input_[index_], std::mem_fn(&gpu_frame::lock));
		writing_[index_] = input_[index_];	
		input_[index_].clear();
		
		// 1. Copy to page-locked memory
		BOOST_FOREACH(auto frame, frames)
		{
			gpu_frame_ptr internal_frame;
			if(frame->tag() == this)
			{ // It is a gpu frame, no more work required
				internal_frame = std::static_pointer_cast<gpu_frame>(frame);
			}
			else
			{ // It is not a gpu frame, create a gpu frame and copy data
				internal_frame = create_frame(frame->width(), frame->height());
				copy_frame(internal_frame, frame);
			}

			input_[next_index].push_back(internal_frame);
		}
				
		// 4. Read from page-locked memory into system memory
		reading_[index_]->read_to_memory(output_frame_->data());

		// Output to external buffer
		finished_frames_.push(output_frame_);
		
		// 3. Draw to framebuffer and start asynchronous DMA transfer to page-locked memory

		// Clear framebuffer
		glClear(GL_COLOR_BUFFER_BIT);	

		// Draw all frames to framebuffer
		boost::range::for_each(writing_[next_index], std::mem_fn(&gpu_frame::draw));

		// Read from framebuffer into page-locked memory
		reading_[next_index]->read_to_pbo(GL_COLOR_ATTACHMENT0_EXT);

		// Unlock frames and give back pointer ownership
		boost::range::for_each(writing_[next_index], std::mem_fn(&gpu_frame::unlock));

		// Create an output frame
		output_frame_ = std::make_shared<system_frame>(format_desc_);

		// Copy audio from composite frames into output frame
		boost::range::for_each(writing_[next_index], std::bind(&copy_frame_audio<frame_ptr>, output_frame_, std::placeholders::_1));	

		// Return frames to pool
		writing_[next_index].clear();
	}

	bool try_pop(frame_ptr& frame)
	{
		return finished_frames_.try_pop(frame);
	}
		
	void init()	
	{
		this_thread_ = boost::this_thread::get_id();
		context_.reset(new sf::Context());
		context_->SetActive(true);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);			
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glViewport(0, 0, format_desc_.width, format_desc_.height);
		glLoadIdentity();

		input_.resize(2);
		writing_.resize(2);
		reading_.resize(2);
		reading_[0] = std::make_shared<common::gpu::pixel_buffer>(format_desc_.width, format_desc_.height);
		reading_[1] = std::make_shared<common::gpu::pixel_buffer>(format_desc_.width, format_desc_.height);
		fbo_ = std::make_shared<frame_buffer>(format_desc_.width, format_desc_.height);
		output_frame_ = std::make_shared<system_frame>(format_desc_);
		index_ = 0;
	}

	void run()
	{
		init();

		std::function<void()> func;
		execution_queue_.pop(func);

		while(func != nullptr)
		{
			func();
			execution_queue_.pop(func);
		}
	}

	gpu_frame_ptr create_frame(size_t width, size_t height)
	{
		size_t key = width | (height << 16);
		auto& pool = frame_pools_[key];

		if(pool.empty())
		{
			auto allocator = [=]
			{
				auto frame = std::make_shared<gpu_frame>(width, height, this);
				frame->unlock();
				frame_pools_[key].push(frame);
			};

			// Always do OpenGL operations on local thread with OpenGL context
			if(boost::this_thread::get_id() == this_thread_)
				allocator();
			else
				execution_queue_.push(allocator);
		}

		gpu_frame_ptr frame;
		pool.pop(frame); // Blocking

		auto destructor = [=]
		{
			frame->audio_data().clear();
			frame_pools_[key].push(frame);
		};

		return gpu_frame_ptr(frame.get(), [=](gpu_frame*)
		{
			// Always do OpenGL operations on local thread with OpenGL context
			if(boost::this_thread::get_id() == this_thread_)
				destructor();
			else
				execution_queue_.push(destructor);
		});
	}

	tbb::concurrent_unordered_map<size_t, tbb::concurrent_bounded_queue<gpu_frame_ptr>> frame_pools_;
		
	frame_buffer_ptr fbo_;
	
	boost::thread::id this_thread_;

	int index_;
	std::vector<std::vector<gpu_frame_ptr>>		input_;
	std::vector<std::vector<gpu_frame_ptr>>		writing_;

	std::vector<common::gpu::pixel_buffer_ptr>	reading_;
	frame_ptr									output_frame_;
	tbb::concurrent_bounded_queue<frame_ptr>	finished_frames_;
		
	std::unique_ptr<sf::Context> context_;
	boost::thread thread_;

	tbb::concurrent_bounded_queue<std::function<void()>> execution_queue_;

	frame_format_desc format_desc_;
	frame_ptr empty_frame_;
};
	
frame_processor::frame_processor(const frame_format_desc& format_desc) : impl_(new implementation(format_desc)){}
void frame_processor::push(const std::vector<frame_ptr>& frames){ impl_->composite(frames);}
bool frame_processor::try_pop(frame_ptr& frame){ return impl_->try_pop(frame);}
frame_ptr frame_processor::create_frame(size_t width, size_t height){return impl_->create_frame(width, height);}

}}
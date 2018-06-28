#pragma once
#include <common/memory.h>

#include <core/fwd.h>
#include <core/frame/draw_frame.h>
#include <core/frame/frame_factory.h>
#include <core/producer/frame_producer.h>
#include <core/interaction/interaction_event.h>
#include <core/frame/frame.h>
#include <core/frame/pixel_format.h>
#include <core/frame/audio_channel_layout.h>
#include <core/frame/geometry.h>

#include <cef_app.h>
#include <cef_client.h>
#include <cef_render_handler.h>
#include <cef_display_handler.h>

#include <boost/thread/mutex.hpp>

#define TBB_PREVIEW_MEMORY_POOL 1
#include <tbb/memory_pool.h>

#include "tbb-memory-chunk-provider.h"

#include <string>
#include <atomic>
#include <deque>

namespace caspar { namespace html {

class browserClient: public CefClient, public CefRenderHandler, public CefLifeSpanHandler, public CefLoadHandler /*, public CefDisplayHandler*/ {
private:
	CefRefPtr<CefBrowser> m_browser;
	spl::shared_ptr<core::frame_factory> m_frame_factory;
	core::video_format_desc m_format_desc;

	std::wstring m_url;
	std::atomic<bool> m_removed;
	std::atomic<bool> m_loaded;

	struct framequeue {
		typedef struct frame {
			int width, height;
			size_t len;
			char buf[1];
		} frame;

		std::deque<frame*> queue;
		boost::mutex lock;
	} m_rawframequeue;

	// Maximum Frames to keep in rawframequeue if chrome tries to push more frames, the oldest one will be dropped.
	// Note: as caspar does not 'boost' the fps in order to keep up; increasing this value 
	//       would'nt make any sense, as the in-queue frames would just increase the delay as they'd be keep'd forever.
	const size_t cMaxNumInRawQueue = 2;


	// Frame Allocator -> TBB Memory Pool, to reduce crt/stl allocator stress,
	// as there would be at least 'fps' * allocations/frees per second.
	class {
		typedef struct { char t[16 * 1024 * 1024]; } tMemoryChunk; // 16M Seems a reasonable value; ~8M = 1080p 32bpp
		tbb::memory_pool< tbbMemoryChunkProvider<tMemoryChunk> > allocator;

	public:
		framequeue::frame *alloc(size_t len) {
			return (framequeue::frame*)allocator.malloc(sizeof(struct framequeue::frame) + len);
		}

		void free(framequeue::frame *rawframe) {
			allocator.free((void*)rawframe);
		}

		void recycle() {
			allocator.recycle();
		}
	} m_rawframeallocator;


public:
	browserClient(spl::shared_ptr<core::frame_factory> frame_factory,
				  const core::video_format_desc& format_desc,
				  const std::wstring& url);
	~browserClient();

	// Closes this browser view
	void close();
	
	// Queue/Schedule removal of this view.
	void remove(); 

	// is removed?
	bool is_removed();

	// Recieves a frame from it's internal frame queue
	core::draw_frame getFrame();

	// Invokes 'requestAnimationframe()' on the browses; in order to force-update the view
	void invoke_requested_animation_frames();

	//
	// CefClient
	//
	bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) override;

	//
	// CefRenderHandler
	//
	CefRefPtr<CefRenderHandler> GetRenderHandler() override;

	virtual bool GetViewRect(
		CefRefPtr<CefBrowser> browser,
		CefRect &rect) override;

	virtual void OnPaint(
		CefRefPtr<CefBrowser> browser,
		PaintElementType type,
		const RectList &dirtyRects,
		const void *buffer,
		int width,
		int height) override;


	//
	// CefLoadHandler 
	//
	CefRefPtr<CefLoadHandler> GetLoadHandler() override;

	virtual void OnLoadEnd(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int httpStatusCode) override;

	//
	// CefLifeSpanHandler 
	//
	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;

	virtual bool OnBeforePopup(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		const CefString &target_url,
		const CefString &target_frame_name,
		WindowOpenDisposition target_disposition,
		bool user_gesture,
		const CefPopupFeatures &popupFeatures,
		CefWindowInfo &windowInfo,
		CefRefPtr<CefClient> &client,
		CefBrowserSettings &settings,
		bool *no_javascript_access) override;


	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

	//
	// CefDisplayHandler
	//
	/*
	virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
								  cef_log_severity_t level,
								  const CefString &message,
								  const CefString &source,
								  int line) override;
								  */

	IMPLEMENT_REFCOUNTING(browserClient);
};

}}
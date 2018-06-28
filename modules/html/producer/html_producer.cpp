/*
* Copyright 2013 Sveriges Television AB http://casparcg.com/
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Robert Nagy, ronag89@gmail.com
*/

#include "html_producer.h"

#include <core/video_format.h>

#include <core/monitor/monitor.h>
#include <core/frame/draw_frame.h>
#include <core/frame/frame_factory.h>
#include <core/producer/frame_producer.h>
#include <core/interaction/interaction_event.h>
#include <core/frame/frame.h>
#include <core/frame/pixel_format.h>
#include <core/frame/audio_channel_layout.h>
#include <core/frame/geometry.h>
#include <core/help/help_repository.h>
#include <core/help/help_sink.h>

#include <common/assert.h>
#include <common/env.h>
#include <common/executor.h>
#include <common/lock.h>
#include <common/future.h>
#include <common/diagnostics/graph.h>
#include <common/prec_timer.h>
#include <common/linq.h>
#include <common/os/filesystem.h>
#include <common/timer.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/ptree.hpp>

#include <tbb/atomic.h>
#include <tbb/concurrent_queue.h>

#pragma warning(push)
#pragma warning(disable: 4458)
#include <cef_task.h>
#include <cef_app.h>
#include <cef_client.h>
#include <cef_render_handler.h>
#pragma warning(pop)

#include <queue>

#include "../html.h"
#include "../browser-client.h"

namespace caspar { namespace html {

class html_producer
	: public core::frame_producer_base
{
	core::monitor::subject	monitor_subject_;
	const std::wstring		url_;
	core::constraints		constraints_;

	CefRefPtr<browserClient>	client_;

public:
	html_producer(
		const spl::shared_ptr<core::frame_factory>& frame_factory,
		const core::video_format_desc& format_desc,
		const std::wstring& url)
		: url_(url)
	{
		constraints_.width.set(format_desc.square_width);
		constraints_.height.set(format_desc.square_height);

		html::invoke([&]
		{
			client_ = new browserClient(frame_factory, format_desc, url_);

			CefWindowInfo window_info;

			window_info.width = format_desc.square_width;
			window_info.height = format_desc.square_height;
			window_info.windowless_rendering_enabled = true;

			CefBrowserSettings browser_settings;
			browser_settings.web_security = cef_state_t::STATE_DISABLED;
			//browser_settings.webgl = cef_state_t::STATE_ENABLED;
			browser_settings.webgl = cef_state_t::STATE_DISABLED;
			double fps = format_desc.fps;
			if (format_desc.field_mode != core::field_mode::progressive) {
				fps *= 2.0;
			}
			browser_settings.windowless_frame_rate = int(ceil(fps));
			CefBrowserHost::CreateBrowser(window_info, client_.get(), url, browser_settings, nullptr);
		});
	}

	~html_producer()
	{
		printf(" -> destroy html_producer!\n");
		if (client_)
			client_->close();
	}

	// frame_producer

	std::wstring name() const override
	{
		return L"html";
	}

	void on_interaction(const core::interaction_event::ptr& event) override
	{
		/*
		if (core::is<core::mouse_move_event>(event))
		{
			auto move = core::as<core::mouse_move_event>(event);
			int x = static_cast<int>(move->x * constraints_.width.get());
			int y = static_cast<int>(move->y * constraints_.height.get());

			CefMouseEvent e;
			e.x = x;
			e.y = y;
			client_->get_browser_host()->SendMouseMoveEvent(e, false);
		}
		else if (core::is<core::mouse_button_event>(event))
		{
			auto button = core::as<core::mouse_button_event>(event);
			int x = static_cast<int>(button->x * constraints_.width.get());
			int y = static_cast<int>(button->y * constraints_.height.get());

			CefMouseEvent e;
			e.x = x;
			e.y = y;
			client_->get_browser_host()->SendMouseClickEvent(
					e,
					static_cast<CefBrowserHost::MouseButtonType>(button->button),
					!button->pressed,
					1);
		}
		else if (core::is<core::mouse_wheel_event>(event))
		{
			auto wheel = core::as<core::mouse_wheel_event>(event);
			int x = static_cast<int>(wheel->x * constraints_.width.get());
			int y = static_cast<int>(wheel->y * constraints_.height.get());

			CefMouseEvent e;
			e.x = x;
			e.y = y;
			static const int WHEEL_TICKS_AMPLIFICATION = 40;
			client_->get_browser_host()->SendMouseWheelEvent(
					e,
					0,                                               // delta_x
					wheel->ticks_delta * WHEEL_TICKS_AMPLIFICATION); // delta_y
		}
		*/
	}

	bool collides(double x, double y) const override
	{
		return true;
	}

	core::draw_frame receive_impl() override
	{
		if (client_)
		{
			if (client_->is_removed())
			{
				client_ = nullptr;
				return core::draw_frame::empty();
			}

			return client_->getFrame();
		}

		return core::draw_frame::empty();
	}

	std::future<std::wstring> call(const std::vector<std::wstring>& params) override
	{
		//wprintf(L"Call: %s\n", params.at(0).c_str());
		if (!client_)
			return make_ready_future(std::wstring(L""));

		auto javascript = params.at(0);

		//client_->execute_javascript(javascript);

		return make_ready_future(std::wstring(L""));
	}

	std::wstring print() const override
	{
		return L"html[" + url_ + L"]";
	}

	boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"html-producer");
		return info;
	}

	core::constraints& pixel_constraints() override
	{
		return constraints_;
	}

	core::monitor::subject& monitor_output()
	{
		return monitor_subject_;
	}
};

void describe_producer(core::help_sink& sink, const core::help_repository& repo)
{
	sink.short_description(L"Renders a web page in real time.");
	sink.syntax(L"{[html_filename:string]},{[HTML] [url:string]}");
	sink.para()->text(L"Embeds an actual web browser and renders the content in realtime.");
	sink.para()
		->text(L"HTML content can either be stored locally under the ")->code(L"templates")
		->text(L" folder or fetched directly via an URL. If a .html file is found with the name ")
		->code(L"html_filename")->text(L" under the ")->code(L"templates")->text(L" folder it will be rendered. If the ")
		->code(L"[HTML] url")->text(L" syntax is used instead, the URL will be loaded.");
	sink.para()->text(L"Examples:");
	sink.example(L">> PLAY 1-10 [HTML] http://www.casparcg.com");
	sink.example(L">> PLAY 1-10 folder/html_file");
}

spl::shared_ptr<core::frame_producer> create_producer(
		const core::frame_producer_dependencies& dependencies,
		const std::vector<std::wstring>& params)
{
	const auto filename			= env::template_folder() + params.at(0) + L".html";
	const auto found_filename	= find_case_insensitive(filename);
	const auto html_prefix		= boost::iequals(params.at(0), L"[HTML]");

	if (!found_filename && !html_prefix)
		return core::frame_producer::empty();

	const auto url = found_filename
		? L"file://" + *found_filename
		: params.at(1);

	if (!html_prefix && (!boost::algorithm::contains(url, ".") || boost::algorithm::ends_with(url, "_A") || boost::algorithm::ends_with(url, "_ALPHA")))
		return core::frame_producer::empty();

	return core::create_destroy_proxy(spl::make_shared<html_producer>(
			dependencies.frame_factory,
			dependencies.format_desc,
			url));
}

}}

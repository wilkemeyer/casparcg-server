#include "browser-client.h"
#include "html.h"

#include <core/frame/frame_factory.h>

namespace caspar { namespace html  {

browserClient::browserClient(spl::shared_ptr<core::frame_factory> frame_factory,
			  const core::video_format_desc& format_desc,
			  const std::wstring& url) 
	: m_frame_factory(std::move(frame_factory)) {

	m_format_desc = format_desc;
	m_url = url;

	m_removed.store(false);
	m_loaded.store(false);

	//printf("-> CONSTRUCT\n");

}//end: constructor


browserClient::~browserClient() {
	//printf("-> DESTRUCT\n");


	m_rawframequeue.queue.clear();
	m_rawframeallocator.recycle();

}//end: destructor


bool browserClient::is_removed() {
	return m_removed.load();
}//end: is_removed()

void browserClient::close() {
	//printf("-> CLOSE\n");

	html::invoke([=] {
		if (m_browser != nullptr) {
			m_browser->GetHost()->CloseBrowser(true);
		}
		else {
			printf("->>> CLOSE BEFORE AFTER CREATE WAS DONE\n");
		}
	});

}//end: close()

void browserClient::remove() {
//	printf("-> REMOVE! (browserClient)\n");

	close();
	m_removed.store(true);

}//end: remove()


bool browserClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message) {

	auto name = message->GetName().ToString();

	if (name == REMOVE_MESSAGE_NAME) {
		// REMOVE MESSAGE
		//
		
		this->remove();
		return true;
	}

	// @TODO
	return false;

}//end: OnProcessMessageReceived()


bool browserClient::GetViewRect(CefRefPtr<CefBrowser> browser,
								CefRect &rect) {

	rect = CefRect(0, 0, m_format_desc.square_width, m_format_desc.square_height);
	return true;

}//end: GetViewRect (CefRenderHandler)


void browserClient::OnPaint(CefRefPtr<CefBrowser> browser,
							PaintElementType type,
							const RectList &dirtyRects,
							const void *buffer,
							int width,
							int height) {

	size_t rgba_length = width * height * 4;
	auto frame = m_rawframeallocator.alloc(rgba_length);
	
	frame->width = width;
	frame->height = height;
	frame->len = rgba_length;
	memcpy(frame->buf, buffer, rgba_length);

	m_rawframequeue.lock.lock();

	// Take care about pending frames in queue 
	if (m_rawframequeue.queue.size() > cMaxNumInRawQueue) {

		// Will happen quite often, commented out verbose info.
		//  printf("-> Dropped one frame as queue already contains > %u frames\n", (unsigned int)cMaxNumInRawQueue);
		
		auto oldframe = m_rawframequeue.queue.back();
		m_rawframeallocator.free(oldframe);

		m_rawframequeue.queue.pop_back();
	}

	// Insert this frame in our queue
	m_rawframequeue.queue.push_back(frame);

	m_rawframequeue.lock.unlock();

}//end: OnPaint (CefRenderHandler)


core::draw_frame browserClient::getFrame() {
	framequeue::frame *rawframe;

	invoke_requested_animation_frames();

	m_rawframequeue.lock.lock(); // @TODO -> trylock!
	size_t queueSize = m_rawframequeue.queue.size();
	if (queueSize == 0) {
		m_rawframequeue.lock.unlock();
		return core::draw_frame::empty();
	}

	rawframe = m_rawframequeue.queue.front();
	
	if (queueSize > 1) {	// Drop frame if there's anothre frame waiting
		m_rawframequeue.queue.pop_front();
	}

	m_rawframequeue.lock.unlock();


	// Got Frame, upload to gpu;	
	core::pixel_format_desc format;
	format.format = core::pixel_format::bgra;
	format.planes.push_back(core::pixel_format_desc::plane(rawframe->width, rawframe->height, 4));

	auto cgframe = m_frame_factory->create_frame(this, format, core::audio_channel_layout::invalid());
	memcpy(cgframe.image_data().begin(), rawframe->buf, rawframe->len);

	if (queueSize > 1) {
		// Free rawframe as we've dropped it from queue if there's more than 1 frame (tripple buffering)
		m_rawframeallocator.free(rawframe);
	}

	return core::draw_frame(std::move(cgframe));
}//end: getFrame()


void browserClient::invoke_requested_animation_frames() {

	if (m_browser) {
		m_browser->SendProcessMessage(CefProcessId::PID_RENDERER, CefProcessMessage::Create(TICK_MESSAGE_NAME));
	}

}//end: invoke_requested_animation_frames()


bool browserClient::OnBeforePopup (
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
	bool *no_javascript_access) {


	//printf(" -> OnBeforePopup\n");
	// Block Popups
	return true;
}//end: OnBeforePopup (CefLifeSpanHandler)


void browserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	//printf(" -> OnAfterCrated\n");
	m_browser = browser;
}//end: OnAfterCreated() (CefLifeSpanhandler)


void browserClient::OnLoadEnd(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	int httpStatusCode) {

	//printf("-> On Load end!!\n");

	m_loaded.store(true);

}//end: OnLoadend (CefLoadHandler)


CefRefPtr<CefRenderHandler> browserClient::GetRenderHandler() {
	return this;
}//end: GetRenderHandler()

CefRefPtr<CefLifeSpanHandler> browserClient::GetLifeSpanHandler() {
	return this;
}//end: GetLifeSpanHandler()

CefRefPtr<CefLoadHandler> browserClient::GetLoadHandler() {
	return this;
}//end: GetLoadHandler()


/*
bool browserClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
									 cef_log_severity_t level,
									 const CefString &message,
									 const CefString &source,
									 int line) {
	printf("-> OnConsoleMessage\n");
	return false; // @TODO -> log.
}//end: OnConsoleMessage (CefDisplayHandler)
*/
}}

#include "html.h"
#include "browser-app.h"

#pragma warning(disable: 4458)
#include <cef_app.h>
#include <cef_version.h>

namespace caspar { namespace html {

static void caspar_log(
	const CefRefPtr<CefBrowser>& browser,
	boost::log::trivial::severity_level level,
	const std::string& message)
{
	if (browser)
	{
		auto msg = CefProcessMessage::Create(LOG_MESSAGE_NAME);
		msg->GetArgumentList()->SetInt(0, level);
		msg->GetArgumentList()->SetString(1, message);
		browser->SendProcessMessage(PID_BROWSER, msg);
	}
}

//
// Remove Handler (V8 JS Handler for remove() method)
// As Caspar calls via proxy  remove()
//

class remove_handler: public CefV8Handler
{
	CefRefPtr<CefBrowser> browser_;
public:
	remove_handler(CefRefPtr<CefBrowser> browser)
		: browser_(browser)
	{
	}

	bool Execute(
		const CefString& name,
		CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception) override
	{
		if (!CefCurrentlyOn(TID_RENDERER))
			return false;

		browser_->SendProcessMessage(
			PID_BROWSER,
			CefProcessMessage::Create(REMOVE_MESSAGE_NAME));

		return true;
	}

	IMPLEMENT_REFCOUNTING(remove_handler);
};



//
// Implementation
//

CefRefPtr<CefRenderProcessHandler> browserApp::GetRenderProcessHandler() {
	return this;
}


void browserApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
}


void browserApp::OnBeforeCommandLineProcessing(const CefString &process_type, CefRefPtr<CefCommandLine> command_line) {
	printf("->OnBeforeCommandLineProcessing\n");

	command_line->AppendSwitch("enable-begin-frame-scheduling");
	command_line->AppendSwitch("enable-media-stream");

	//if (process_type.empty()) {
		command_line->AppendSwitch("disable-gpu");
		command_line->AppendSwitch("disable-gpu-compositing");
		command_line->AppendSwitchWithValue("disable-gpu-vsync", "gpu");
	//}
}


void browserApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
	caspar_log(browser, boost::log::trivial::trace,"context for frame " + boost::lexical_cast<std::string>(frame->GetIdentifier()) + " created");
	
	m_contexts.push_back(context);

	// Add remove() method 

	auto window = context->GetGlobal();
	window->SetValue(
		"remove",
		CefV8Value::CreateFunction(
		"remove",
		new remove_handler(browser)),
		V8_PROPERTY_ATTRIBUTE_NONE);

	// @ TODO -> Animation Frame stuff


}//end: OnContextCreated()


void browserApp::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {

	auto removed = boost::remove_if(m_contexts, [&](const CefRefPtr<CefV8Context>& c) {
		return c->IsSame(context);
	});

	if (removed != m_contexts.end())
		caspar_log(browser, boost::log::trivial::trace, "context for frame " + boost::lexical_cast<std::string>(frame->GetIdentifier()) + " released");
	else
		caspar_log(browser, boost::log::trivial::warning,"context for frame " + boost::lexical_cast<std::string>(frame->GetIdentifier()) + " released, but not found");
	
}//end: OnContextReleased()


void browserApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
	m_contexts.clear();
}//end: OnBrowserDestroyed()


bool browserApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
	printf("@App OnProcessMessageReceived\n");
	// Used as messagebus handler to process messages sent by externel code or views

	return false;
}


/*
bool browserApp::Execute(const CefString &name, CefRefPtr<CefV8Value> object, const CefV8ValueList &arguments, CefRefPtr<CefV8Value> &retval, CefString &exception) {
	printf("@App Execute\n");
	return false;
}*/


void browserApp::ExecuteJSFunction(CefRefPtr<CefBrowser> browser, const char *functionName, CefV8ValueList arguments) {
}


}}
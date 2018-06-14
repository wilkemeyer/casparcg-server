#pragma once

#include <cef_app.h>

namespace caspar { namespace html {

class browserApp : public CefApp, public CefRenderProcessHandler /*, public CefV8Handler  */ {
public:

	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
	virtual void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
	virtual void OnBeforeCommandLineProcessing(const CefString &process_type,CefRefPtr<CefCommandLine> command_line) override;
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
	virtual void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;
	virtual void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;
//	virtual bool Execute(const CefString &name, CefRefPtr<CefV8Value> object, const CefV8ValueList &arguments, CefRefPtr<CefV8Value> &retval, CefString &exception) override;


private:
	void ExecuteJSFunction(CefRefPtr<CefBrowser> browser, const char *functionName, CefV8ValueList arguments);


	std::vector<CefRefPtr<CefV8Context>> m_contexts;

	IMPLEMENT_REFCOUNTING(browserApp);
};

}}
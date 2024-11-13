#include "include/cef_app.h"

// Implement application-level callbacks for the browser process.
class SubprocessApp : public CefApp, public CefRenderProcessHandler {
 public:
  SubprocessApp();

  // CefApp methods:
  virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler()
      override {
    return this;
  }

  // CefBrowserProcessHandler methods:
  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) override;

 private:
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(SubprocessApp);
};
#include "subprocess_app.h"
#include "executor.h"

SubprocessApp::SubprocessApp() {}

void SubprocessApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context) {
  AllocConsole();
  freopen_s((FILE**)stdout, "con", "w", (FILE*)stdout);

  CefRefPtr<CefV8Handler> handler = new FunctionExecutor(this);

  context->GetGlobal()->SetValue(
      "setInteractionMode",
      CefV8Value::CreateFunction("setInteractionMode", handler),
      V8_PROPERTY_ATTRIBUTE_NONE);
}
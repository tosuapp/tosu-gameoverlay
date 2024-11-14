#include "executor.h"

FunctionExecutor::FunctionExecutor(CefRefPtr<CefApp> app) {
  this->app = app;
}

bool FunctionExecutor::Execute(const CefString& name,
                               CefRefPtr<CefV8Value> object,
                               const CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               CefString& exception) {
  if (name.ToString() == "setInteractionMode" && arguments.size() == 1) {
    // clang-ignore
    auto hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(
        object->GetValue("mainProcessHwnd")->GetIntValue()));

    auto enabled = arguments[0]->GetBoolValue();

    PostMessageA(hwnd, WM_USER + 1, 0, enabled);
  }

  return false;
}
#pragma once

#include "include/cef_app.h"
#include "include/cef_v8.h"

struct FunctionExecutor : public CefV8Handler {
  FunctionExecutor(CefRefPtr<CefApp> app);

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override;

 private:
  CefRefPtr<CefApp> app;

  IMPLEMENT_REFCOUNTING(FunctionExecutor);
};
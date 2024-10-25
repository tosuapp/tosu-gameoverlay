#pragma once

#include "include/cef_browser.h"

namespace input {

void initialize(HWND hwnd, CefRefPtr<CefBrowser> browser);

}  // namespace input

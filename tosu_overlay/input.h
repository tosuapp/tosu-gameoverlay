#pragma once

#include "include/cef_browser.h"

namespace input {

void initialize(HWND hwnd,
                uint32_t main_thread_id,
                CefRefPtr<CefBrowser> browser);

}  // namespace input

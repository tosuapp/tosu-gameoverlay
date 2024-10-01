// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tosu_overlay/tosu_overlay_app.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/internal/cef_types_runtime.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "tosu_overlay/tosu_overlay_handler.h"

namespace {

// When using the Views framework this object provides the delegate
// implementation for the CefWindow that hosts the Views-based browser.
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                       cef_runtime_style_t runtime_style,
                       cef_show_state_t initial_show_state)
      : browser_view_(browser_view),
        runtime_style_(runtime_style),
        initial_show_state_(initial_show_state) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    // Add the browser view and show the window.
    window->AddChildView(browser_view_);

    if (initial_show_state_ != CEF_SHOW_STATE_HIDDEN) {
      window->Show();
    }

    if (initial_show_state_ != CEF_SHOW_STATE_MINIMIZED &&
        initial_show_state_ != CEF_SHOW_STATE_HIDDEN) {
      // Give keyboard focus to the browser view.
      browser_view_->RequestFocus();
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK.
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override {
    return initial_show_state_;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  const cef_runtime_style_t runtime_style_;
  const cef_show_state_t initial_show_state_;

  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit SimpleBrowserViewDelegate(cef_runtime_style_t runtime_style)
      : runtime_style_(runtime_style) {}

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    // Create a new top-level Window for the popup. It will show itself after
    // creation.
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        popup_browser_view, runtime_style_, CEF_SHOW_STATE_NORMAL));

    // We created the Window.
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  const cef_runtime_style_t runtime_style_;

  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};

}  // namespace

TosuOverlay::TosuOverlay() = default;

void TosuOverlay::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // #if !defined(DISABLE_ALLOY_BOOTSTRAP)
  //   const bool enable_chrome_runtime =
  //       !command_line->HasSwitch("disable-chrome-runtime");
  // #endif

  bool use_alloy_style = true;
  cef_runtime_style_t runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  // SimpleHandler implements browser-level callbacks.
  CefRefPtr<SimpleHandler> handler(new SimpleHandler(use_alloy_style));

  // Specify CEF browser settings here.
  CefBrowserSettings browser_settings;

  browser_settings.windowless_frame_rate = 60;

  std::string url;

  // Check if a "--url=" value was provided via the command-line. If so, use
  // that instead of the default URL.
  url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "http://127.0.0.1:24050/tosu-debug by cyperdark/";
  }

  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      handler, url, browser_settings, nullptr, nullptr,
      new SimpleBrowserViewDelegate(runtime_style));

  CefWindowInfo window_info;

  window_info.SetAsWindowless(GetDesktopWindow());
  window_info.windowless_rendering_enabled = TRUE;
  window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  CefBrowserHost::CreateBrowserSync(window_info, handler, url, browser_settings,
                                    nullptr, nullptr);

  // CefBrowserHost::CreateBrowserSync(window_info, handler,
  // "https://google.com",
  //                                   browser_settings, nullptr, nullptr);

  // CefBrowserHost::CreateBrowserSync(window_info, handler, "https://ya.com",
  //                                   browser_settings, nullptr, nullptr);

  // CefBrowserHost::CreateBrowserSync(window_info, handler, "https://tosu.app",
  //                                   browser_settings, nullptr, nullptr);
}

CefRefPtr<CefClient> TosuOverlay::GetDefaultClient() {
  // Called when a new browser window is created via the Chrome runtime UI.
  return SimpleHandler::GetInstance();
}

void TosuOverlay::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  command_line->AppendSwitch("disable-web-security");
  command_line->AppendSwitch("enable-begin-frame-scheduling");
  command_line->AppendSwitchWithValue("remote-allow-origins", "*");

  command_line->AppendSwitch("disable-gpu");
  command_line->AppendSwitch("disable-gpu-compositing");
  command_line->AppendSwitchWithValue("disable-gpu-vsync", "gpu");

  command_line->AppendSwitchWithValue("remote-debugging-port", "9222");
  command_line->AppendSwitch("ignore-certificate-errors");
}
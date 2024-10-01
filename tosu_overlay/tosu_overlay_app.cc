// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tosu_overlay/tosu_overlay_app.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/internal/cef_types_runtime.h"
#include "include/wrapper/cef_helpers.h"
#include "tosu_overlay/tosu_overlay_handler.h"


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
    url = "http://127.0.0.1:24050/osuUserStats by HosizoraN/";
  }

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
  command_line->AppendSwitch("enable-experimental-web-platform-features");
  command_line->AppendSwitch("in-process-gpu");
  command_line->AppendSwitch("enable-media-stream");
  command_line->AppendSwitch("use-fake-ui-for-media-stream");
  command_line->AppendSwitch("enable-speech-input");
  command_line->AppendSwitch("ignore-gpu-blacklist");
  command_line->AppendSwitch("enable-usermedia-screen-capture");

  command_line->AppendSwitchWithValue("default-encoding", "utf-8");

  command_line->AppendSwitch("disable-gpu");
  command_line->AppendSwitch("disable-gpu-compositing");
  command_line->AppendSwitch("enable-begin-frame-scheduling");

  command_line->AppendSwitch("disable-web-security");
  command_line->AppendSwitchWithValue("remote-allow-origins", "*");
  command_line->AppendSwitchWithValue("remote-debugging-port", "9222");
  command_line->AppendSwitch("ignore-certificate-errors");
}
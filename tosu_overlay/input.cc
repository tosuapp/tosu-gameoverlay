#include <Windows.h>
#include <tosu_overlay/input.h>
#include <tosu_overlay/tosu_overlay_handler.h>
#include <windowsx.h>
#include <thread>
#include "executor.h"
#include "include/cef_v8.h"

// Thanks !!! :D
// https://github.com/ONLYOFFICE/desktop-sdk/blob/master/ChromiumBasedEditors/lib/src/cef/windows/tests/cefclient/browser/osr_window_win.cc

namespace {

bool edit_mode = false;
uint32_t main_thread;

int last_click_x_;
int last_click_y_;
CefBrowserHost::MouseButtonType last_click_button_;
int last_click_count_;
double last_click_time_;
float device_scale_factor_ = 1.0f;

HWND window_handle = 0;

CefRefPtr<CefBrowser> cef_browser;

int DeviceToLogical(int value, float device_scale_factor) {
  float scaled_val = static_cast<float>(value) / device_scale_factor;
  return static_cast<int>(std::floor(scaled_val));
}

void DeviceToLogical(CefMouseEvent& value, float device_scale_factor) {
  value.x = DeviceToLogical(value.x, device_scale_factor);
  value.y = DeviceToLogical(value.y, device_scale_factor);
}

bool IsKeyDown(WPARAM wparam) {
  return (GetKeyState(wparam) & 0x8000) != 0;
}

int GetCefMouseModifiers(WPARAM wparam) {
  int modifiers = 0;
  if (wparam & MK_CONTROL)
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (wparam & MK_SHIFT)
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (IsKeyDown(VK_MENU))
    modifiers |= EVENTFLAG_ALT_DOWN;
  if (wparam & MK_LBUTTON)
    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  if (wparam & MK_MBUTTON)
    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  if (wparam & MK_RBUTTON)
    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

  // Low bit set from GetKeyState indicates "toggled".
  if (::GetKeyState(VK_NUMLOCK) & 1)
    modifiers |= EVENTFLAG_NUM_LOCK_ON;
  if (::GetKeyState(VK_CAPITAL) & 1)
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;
  return modifiers;
}

int GetCefKeyboardModifiers(WPARAM wparam, LPARAM lparam) {
  int modifiers = 0;
  if (IsKeyDown(VK_SHIFT))
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (IsKeyDown(VK_CONTROL))
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (IsKeyDown(VK_MENU))
    modifiers |= EVENTFLAG_ALT_DOWN;

  // Low bit set from GetKeyState indicates "toggled".
  if (::GetKeyState(VK_NUMLOCK) & 1)
    modifiers |= EVENTFLAG_NUM_LOCK_ON;
  if (::GetKeyState(VK_CAPITAL) & 1)
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;

  switch (wparam) {
    case VK_RETURN:
      if ((lparam >> 16) & KF_EXTENDED)
        modifiers |= EVENTFLAG_IS_KEY_PAD;
      break;
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
      if (!((lparam >> 16) & KF_EXTENDED))
        modifiers |= EVENTFLAG_IS_KEY_PAD;
      break;
    case VK_NUMLOCK:
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
    case VK_DECIMAL:
    case VK_CLEAR:
      modifiers |= EVENTFLAG_IS_KEY_PAD;
      break;
    case VK_SHIFT:
      if (IsKeyDown(VK_LSHIFT))
        modifiers |= EVENTFLAG_IS_LEFT;
      else if (IsKeyDown(VK_RSHIFT))
        modifiers |= EVENTFLAG_IS_RIGHT;
      break;
    case VK_CONTROL:
      if (IsKeyDown(VK_LCONTROL))
        modifiers |= EVENTFLAG_IS_LEFT;
      else if (IsKeyDown(VK_RCONTROL))
        modifiers |= EVENTFLAG_IS_RIGHT;
      break;
    case VK_MENU:
      if (IsKeyDown(VK_LMENU))
        modifiers |= EVENTFLAG_IS_LEFT;
      else if (IsKeyDown(VK_RMENU))
        modifiers |= EVENTFLAG_IS_RIGHT;
      break;
    case VK_LWIN:
      modifiers |= EVENTFLAG_IS_LEFT;
      break;
    case VK_RWIN:
      modifiers |= EVENTFLAG_IS_RIGHT;
      break;
  }
  return modifiers;
}

void on_mouse_event(UINT code, WPARAM wparam, LPARAM lparam) {
  auto browser_host = cef_browser->GetHost();

  LONG currentTime = 0;
  bool cancelPreviousClick = false;

  if (code == WM_LBUTTONDOWN || code == WM_RBUTTONDOWN ||
      code == WM_MBUTTONDOWN || code == WM_MOUSEMOVE || code == WM_MOUSELEAVE) {
    currentTime = GetMessageTime();
    int x = GET_X_LPARAM(lparam);
    int y = GET_Y_LPARAM(lparam);
    cancelPreviousClick =
        (abs(last_click_x_ - x) > (GetSystemMetrics(SM_CXDOUBLECLK) / 2)) ||
        (abs(last_click_y_ - y) > (GetSystemMetrics(SM_CYDOUBLECLK) / 2)) ||
        ((currentTime - last_click_time_) > GetDoubleClickTime());
    if (cancelPreviousClick &&
        (code == WM_MOUSEMOVE || code == WM_MOUSELEAVE)) {
      last_click_count_ = 1;
      last_click_x_ = 0;
      last_click_y_ = 0;
      last_click_time_ = 0;
    }
  }

  if (!edit_mode && code != WM_MOUSEMOVE && code != WM_MOUSELEAVE) {
    return;
  }

  switch (code) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
      ::SetCapture(window_handle);
      ::SetFocus(window_handle);
      int x = GET_X_LPARAM(lparam);
      int y = GET_Y_LPARAM(lparam);

      CefBrowserHost::MouseButtonType btnType =
          (code == WM_LBUTTONDOWN
               ? MBT_LEFT
               : (code == WM_RBUTTONDOWN ? MBT_RIGHT : MBT_MIDDLE));
      if (!cancelPreviousClick && (btnType == last_click_button_)) {
        ++last_click_count_;
      } else {
        last_click_count_ = 1;
        last_click_x_ = x;
        last_click_y_ = y;
      }
      last_click_time_ = currentTime;
      last_click_button_ = btnType;

      CefMouseEvent mouse_event;
      mouse_event.x = x;
      mouse_event.y = y;
      DeviceToLogical(mouse_event, device_scale_factor_);
      mouse_event.modifiers = GetCefMouseModifiers(wparam);
      browser_host->SendMouseClickEvent(mouse_event, btnType, false,
                                        last_click_count_);
    } break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
      if (GetCapture() == window_handle) {
        ReleaseCapture();
      }
      int x = GET_X_LPARAM(lparam);
      int y = GET_Y_LPARAM(lparam);
      CefBrowserHost::MouseButtonType btnType =
          (code == WM_LBUTTONUP
               ? MBT_LEFT
               : (code == WM_RBUTTONUP ? MBT_RIGHT : MBT_MIDDLE));
      CefMouseEvent mouse_event;
      mouse_event.x = x;
      mouse_event.y = y;

      DeviceToLogical(mouse_event, device_scale_factor_);
      mouse_event.modifiers = GetCefMouseModifiers(wparam);
      browser_host->SendMouseClickEvent(mouse_event, btnType, true,
                                        last_click_count_);
    }

    break;

    case WM_MOUSEMOVE: {
      int x = GET_X_LPARAM(lparam);
      int y = GET_Y_LPARAM(lparam);

      CefMouseEvent mouse_event;
      mouse_event.x = x;
      mouse_event.y = y;

      DeviceToLogical(mouse_event, device_scale_factor_);
      mouse_event.modifiers = GetCefMouseModifiers(wparam);
      browser_host->SendMouseMoveEvent(mouse_event, false);
    } break;

    case WM_MOUSELEAVE: {
      POINT p;
      ::GetCursorPos(&p);
      ::ScreenToClient(window_handle, &p);

      CefMouseEvent mouse_event;
      mouse_event.x = p.x;
      mouse_event.y = p.y;
      DeviceToLogical(mouse_event, device_scale_factor_);
      mouse_event.modifiers = GetCefMouseModifiers(wparam);
      browser_host->SendMouseMoveEvent(mouse_event, true);
    } break;

    case WM_MOUSEWHEEL:
      POINT screen_point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      HWND scrolled_wnd = ::WindowFromPoint(screen_point);
      if (scrolled_wnd != window_handle) {
        break;
      }

      ScreenToClient(window_handle, &screen_point);
      int delta = GET_WHEEL_DELTA_WPARAM(wparam);

      CefMouseEvent mouse_event;
      mouse_event.x = screen_point.x;
      mouse_event.y = screen_point.y;

      DeviceToLogical(mouse_event, device_scale_factor_);
      mouse_event.modifiers = GetCefMouseModifiers(wparam);
      browser_host->SendMouseWheelEvent(mouse_event,
                                        IsKeyDown(VK_SHIFT) ? delta : 0,
                                        !IsKeyDown(VK_SHIFT) ? delta : 0);
      break;
  }
}

void on_key_event(UINT code, WPARAM wparam, LPARAM lparam) {
  if (!edit_mode) {
    return;
  }

  CefKeyEvent event;
  event.windows_key_code = wparam;
  event.native_key_code = lparam;
  event.is_system_key =
      code == WM_SYSCHAR || code == WM_SYSKEYDOWN || code == WM_SYSKEYUP;

  if (code == WM_KEYDOWN || code == WM_SYSKEYDOWN)
    event.type = KEYEVENT_RAWKEYDOWN;
  else if (code == WM_KEYUP || code == WM_SYSKEYUP)
    event.type = KEYEVENT_KEYUP;
  else
    event.type = KEYEVENT_CHAR;

  event.modifiers = GetCefKeyboardModifiers(wparam, lparam);

  if ((event.type == KEYEVENT_CHAR) && IsKeyDown(VK_RMENU)) {
    HKL current_layout = ::GetKeyboardLayout(0);

    SHORT scan_res = ::VkKeyScanExW(wparam, current_layout);
    constexpr auto ctrlAlt = (2 | 4);
    if (((scan_res >> 8) & ctrlAlt) == ctrlAlt) {  // ctrl-alt pressed
      event.modifiers &= ~(EVENTFLAG_CONTROL_DOWN | EVENTFLAG_ALT_DOWN);
      event.modifiers |= EVENTFLAG_ALTGR_DOWN;
    }
  }

  cef_browser->GetHost()->SendKeyEvent(event);
}

LONG_PTR original_wnd_proc = 0;

LRESULT __stdcall wnd_proc_hk(HWND hWnd,
                              UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam) {
  if (!edit_mode) {
    return CallWindowProcW(reinterpret_cast<WNDPROC>(original_wnd_proc), hWnd,
                           uMsg, wParam, lParam);
  }

  switch (uMsg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_MOUSEWHEEL:
      if constexpr (sizeof(void*) == 8) {
        on_mouse_event(uMsg, wParam, lParam);
      }
      return 0;
    case WM_SYSCHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
      if constexpr (sizeof(void*) == 8) {
        on_key_event(uMsg, wParam, lParam);
      }
      return 0;
  }

  return CallWindowProcW(reinterpret_cast<WNDPROC>(original_wnd_proc), hWnd,
                         uMsg, wParam, lParam);
}

HHOOK original_get_message;

LRESULT __stdcall get_message(int code, WPARAM wparam, LPARAM lparam) {
  if (wparam == PM_REMOVE) {
    const auto msg_raw = reinterpret_cast<MSG*>(lparam);

    const auto msg = msg_raw->message;
    const auto w_param = msg_raw->wParam;
    const auto l_param = msg_raw->lParam;

    switch (msg) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_RBUTTONUP:
      case WM_MBUTTONUP:
      case WM_MOUSEMOVE:
      case WM_MOUSELEAVE:
      case WM_MOUSEWHEEL:
        on_mouse_event(msg, w_param, l_param);
        break;
      case WM_SYSCHAR:
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_CHAR:
        on_key_event(msg, w_param, l_param);
        break;
      case WM_USER + 1:
        edit_mode = static_cast<bool>(l_param);
        return 0;
    }
  }

  return CallNextHookEx(original_get_message, code, wparam, lparam);
}

// void bindings_thread() {
//   bool last_states[3];

//   while (true) {
//     // ghetto way but whatever
//     const auto is_ctrl_down = GetAsyncKeyState(VK_LCONTROL) != 0;
//     const auto is_shift_down = GetAsyncKeyState(VK_LSHIFT) != 0;
//     const auto is_space_down = GetAsyncKeyState(VK_SPACE) != 0;

//     auto current_state = is_ctrl_down && is_shift_down && is_space_down;
//     auto last_state = last_states[0] && last_states[1] && last_states[2];

//     if (current_state != last_state) {
//       if (current_state) {
//         auto script = edit_mode ? "window.postMessage('editingEnded')"
//                                 : "window.postMessage('editingStarted')";

//         cef_browser->GetMainFrame()->ExecuteJavaScript(script, "", 0);

//         edit_mode = !edit_mode;
//       }
//     }

//     last_states[0] = is_ctrl_down;
//     last_states[1] = is_shift_down;
//     last_states[2] = is_space_down;

//     Sleep(1);
//   }
// }

}  // namespace

void input::initialize(HWND hwnd,
                       uint32_t main_thread_id,
                       CefRefPtr<CefBrowser> browser) {
  main_thread = main_thread_id;
  window_handle = hwnd;
  cef_browser = browser;

  if constexpr (sizeof(void*) == 4) {
    // used for reading input data
    while (!original_get_message) {
      original_get_message = SetWindowsHookExA(            //
          WH_GETMESSAGE, &get_message,                     //
          GetModuleHandleA(nullptr), GetCurrentThreadId()  //
      );
    }
  }

  // used for blocking inputs
  original_wnd_proc = SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                                       reinterpret_cast<LONG_PTR>(wnd_proc_hk));

  cef_browser->GetMainFrame()->ExecuteJavaScript(
      "window.mainProcessHwnd = " +
          std::to_string(reinterpret_cast<uintptr_t>(hwnd)),
      "", 0);

  // std::thread(bindings_thread).detach();
}

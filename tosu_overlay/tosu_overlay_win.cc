#include <windows.h>

#include <include/cef_command_line.h>
#include <include/cef_sandbox_win.h>
#include <tosu_overlay/canvas.h>
#include <tosu_overlay/tosu_overlay_app.h>
#include <tosu_overlay/tools.h>
#include <tosu_overlay/config.h>

#include <MinHook.h>

#include <glad/glad.h>
#include <wingdi.h>

#include <filesystem>
#include <mutex>
#include <thread>

// Uncomment this line to manually enable sandbox support.
// #define CEF_USE_SANDBOX 1

#if defined(CEF_USE_SANDBOX)
#pragma comment(lib, "cef_sandbox.lib")
#endif

namespace {

void initialize_cef(HINSTANCE hInstance) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  auto exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return;
  }

  // Specify CEF global settings here.
  CefSettings settings;

#if !defined(DISABLE_ALLOY_BOOTSTRAP)
  // Parse command-line arguments for use in this method.
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromString(::GetCommandLineW());

  // Use the CEF Chrome bootstrap unless "--disable-chrome-runtime" is specified
  // via the command-line. Otherwise, use the CEF Alloy bootstrap. The Alloy
  // bootstrap is deprecated and will be removed in ~M127. See
  // https://github.com/chromiumembedded/cef/issues/3685
  settings.chrome_runtime = !command_line->HasSwitch("disable-chrome-runtime");
#endif

  settings.windowless_rendering_enabled = true;

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  // TosuOverlay implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  std::wstring module_path = tools::get_module_path(hInstance);
  auto cef_path = std::filesystem::path(module_path).parent_path();

  CefRefPtr<TosuOverlay> app(new TosuOverlay(cef_path.string()));

  auto exe_path =
      cef_path / "tosu_overlay.exe";

  CefString(&settings.browser_subprocess_path) = exe_path;

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    // CefGetExitCode();
    return;
  }

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();
}

#if !DESKTOP

void* o_swap_buffers;
std::once_flag glad_init_flag;

#endif
}  // namespace

#if DESKTOP
// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

#if defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
  auto exit_code = CefRunWinMainWithPreferredStackSize(wWinMain, hInstance,
                                                       lpCmdLine, nCmdShow);
  if (exit_code >= 0) {
    // The fiber has completed so return here.
    return exit_code;
  }
#endif

  initialize_cef(hInstance);

  return 0;
}
#else

bool __stdcall swap_buffers_hk(HDC hdc) {
  std::call_once(glad_init_flag,
                 []() { printf("gl loading result: %d\n", gladLoadGL()); });

  canvas::draw(hdc);

  return reinterpret_cast<decltype(&swap_buffers_hk)>(o_swap_buffers)(hdc);
}

void main_thread(HINSTANCE hInstance) {
  // AllocConsole();
  // freopen_s((FILE**)stdout, "con", "w", (FILE*)stdout);
  std::wstring module_path = tools::get_module_path(hInstance);

  auto parent_path = std::filesystem::path(module_path).parent_path();
  auto config_path = parent_path / "config.json";

  ConfigManager::getInstance(config_path.string());

  auto cef_path = parent_path / "libcef.dll";

  LoadLibraryEx(cef_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  auto last_error = GetLastError();
  if (last_error != 0) {
    printf("%s\n", "Can't load libcef.dll");
    return;
  }

  MH_Initialize();

  MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers",
                   reinterpret_cast<void*>(swap_buffers_hk), &o_swap_buffers);

  MH_EnableHook(MH_ALL_HOOKS);

  std::thread{initialize_cef, hInstance}.detach();
}

int32_t __stdcall DllMain(HINSTANCE hInstance, uint32_t reason, uintptr_t) {
  if (reason == DLL_PROCESS_ATTACH) {
    std::thread{main_thread, hInstance}.detach();
  }
  return true;
}
#endif
#include <windows.h>

#include <include/cef_command_line.h>
#include <include/cef_sandbox_win.h>
#include <tosu_overlay/tosu_overlay_app.h>

#include <MinHook.h>
#include <winnt.h>

#include <filesystem>
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

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  // TosuOverlay implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  CefRefPtr<TosuOverlay> app(new TosuOverlay);

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
  return reinterpret_cast<decltype(&swap_buffers_hk)>(o_swap_buffers)(hdc);
}

void main_thread(HINSTANCE hInstance) {
  AllocConsole();
  freopen_s((FILE**)stdout, "con", "w", (FILE*)stdout);

  wchar_t module_path[MAX_PATH];
  if (GetModuleFileName(hInstance, module_path, sizeof(module_path)) == 0) {
    printf("GetModuleFileName failed, error = %d\n",
           static_cast<int32_t>(GetLastError()));
    return;
  }

  auto cef_path =
      std::filesystem::path(module_path).parent_path() / "libcef.dll";
  printf("%s\n", cef_path.string().c_str());

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
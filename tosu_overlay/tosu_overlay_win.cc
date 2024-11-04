#include <windows.h>

#include <include/cef_command_line.h>
#include <include/cef_sandbox_win.h>
#include <tosu_overlay/canvas.h>
#include <tosu_overlay/config.h>
#include <tosu_overlay/state.h>
#include <tosu_overlay/tools.h>
#include <tosu_overlay/tosu_overlay_app.h>
#include <tosu_overlay/tosu_overlay_handler.h>

#include <MinHook.h>

#include <glad/glad.h>
#include <wingdi.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include <tosu_overlay/input.h>

// Uncomment this line to manually enable sandbox support.
// #define CEF_USE_SANDBOX 1

#if defined(CEF_USE_SANDBOX)
#pragma comment(lib, "cef_sandbox.lib")
#endif

namespace {

bool is_cef_initialized = false;

#if DESKTOP
void initialize_cef_subprocess(HINSTANCE hInstance) {
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
}
#endif

void initialize_cef_dll(HINSTANCE hInstance) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

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

  auto exe_path = cef_path / "tosu_overlay.exe";

  CefString(&settings.browser_subprocess_path) = exe_path;

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    // CefGetExitCode();
    return;
  }

  is_cef_initialized = true;

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();
}

#if !DESKTOP

void* o_swap_buffers;
std::once_flag glad_init_flag;
std::once_flag input_init_flag;

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

  initialize_cef_subprocess(hInstance);

  return 0;
}
#else

bool __stdcall swap_buffers_hk(HDC hdc) {
  std::call_once(glad_init_flag, []() { gladLoadGL(); });

  if (is_cef_initialized) {
    std::call_once(input_init_flag, [hdc]() {
      input::initialize(WindowFromDC(hdc), GetCurrentThreadId(),
                        SimpleHandler::GetInstance()->GetBrowserList().front());
    });
  }

  canvas::draw(hdc);

  return reinterpret_cast<decltype(&swap_buffers_hk)>(o_swap_buffers)(hdc);
}

void parse_tosu_env(std::filesystem::path overlay_dir) {
  if (!overlay_dir.has_parent_path()) {
    return;
  }

  const auto tosu_dir = overlay_dir.parent_path();
  const auto env_path = tosu_dir / "tsosu.env";

  if (!std::filesystem::exists(env_path)) {
    return;
  }

  auto file = std::ifstream(env_path.string());

  std::string line;
  while (std::getline(file, line)) {
    // skip empty lines and comments
    if (line.empty() || line.data()[0] == '#') {
      continue;
    }

    auto delimiter_pos = line.find('=');

    // delimiter not found
    if (delimiter_pos == std::string::npos) {
      continue;
    }

    auto key = line.substr(0, delimiter_pos);
    auto value = line.substr(delimiter_pos + 1, line.length() - delimiter_pos);

    if (key == "SERVER_IP") {
      state::host = value;
    } else if (key == "SERVER_PORT") {
      state::port = value;
    }
  }
}

void main_thread(HINSTANCE hInstance) {
  AllocConsole();
  freopen_s((FILE**)stdout, "con", "w", (FILE*)stdout);

  const auto module_path = tools::get_module_path(hInstance);

  const auto parent_path = std::filesystem::path(module_path).parent_path();
  const auto config_path = parent_path / "config.json";

  parse_tosu_env(parent_path);

  ConfigManager::get_instance(config_path.string());

  const auto cef_path = parent_path / "libcef.dll";

  LoadLibraryEx(cef_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  const auto last_error = GetLastError();
  if (last_error != 0) {
    printf("%s\n", "Can't load libcef.dll");
    return;
  }

  MH_Initialize();

  MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers",
                   reinterpret_cast<void*>(swap_buffers_hk), &o_swap_buffers);

  MH_EnableHook(MH_ALL_HOOKS);

  std::thread{initialize_cef_dll, hInstance}.detach();
}

int32_t __stdcall DllMain(HINSTANCE hInstance, uint32_t reason, uintptr_t) {
  if (reason == DLL_PROCESS_ATTACH) {
    std::thread{main_thread, hInstance}.detach();
  }
  return true;
}
#endif
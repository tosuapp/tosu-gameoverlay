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

#include <tosu_overlay/logger.h>

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

  CefRefPtr<TosuOverlay> app(new TosuOverlay(cef_path.string().c_str()));

  auto exe_path = cef_path / "tosu_overlay.exe";
  logger::log("tosu_overlay exe_path: %s", exe_path.string().c_str());

  CefString(&settings.browser_subprocess_path) = exe_path;

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    logger::log("Failed to initialize CEF (exit code: %d)", CefGetExitCode());
    return;
  }

  is_cef_initialized = true;

  logger::log("CEF initialized successfully");

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
  std::call_once(glad_init_flag, []() {
    logger::log("Initializing GL functions");
    if (auto status = gladLoadGL() == 0) {
      logger::log("Failed to initialize GL functions (status: %d)", status);
    }
  });

  if (is_cef_initialized) {
    std::call_once(input_init_flag, [hdc]() {
      logger::log("Initializing input");
      input::initialize(WindowFromDC(hdc), GetCurrentThreadId(),
                        SimpleHandler::GetInstance()->GetBrowserList().front());
      logger::log("Input initialized");
    });
  }

  canvas::draw(hdc);

  return reinterpret_cast<decltype(&swap_buffers_hk)>(o_swap_buffers)(hdc);
}

void parse_tosu_env(std::filesystem::path overlay_dir) {
  logger::log("Begin parsing env");

  if (!overlay_dir.has_parent_path()) {
    logger::log("No parent path found for overlay_dir");
    return;
  }

  // tosu/game_overlay/<bitness>
  const auto tosu_dir = overlay_dir.parent_path().parent_path();
  const auto env_path = tosu_dir / "tosu.env";

  if (!std::filesystem::exists(env_path)) {
    logger::log("Env path doesn't exist");
    return;
  }

  logger::log("Env path located at %s", env_path.string().c_str());

  auto file = std::ifstream(env_path.string().c_str());

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
      logger::log("SERVER_IP parsed as %s", value.c_str());
      state::host = value;
    } else if (key == "SERVER_PORT") {
      logger::log("SERVER_PORT parsed as %s", value.c_str());
      state::port = value;
    }
  }

  logger::log("Done parsing env");
}

void initialize_logger(std::filesystem::path overlay_dir) {
  const auto log_folder = overlay_dir / "logs";

  if (!std::filesystem::exists(log_folder)) {
    std::filesystem::create_directory(log_folder);
  }

  logger::setup_logger(log_folder);
}

void main_thread(HINSTANCE hInstance) {
  // AllocConsole();
  // freopen_s((FILE**)stdout, "con", "w", (FILE*)stdout);

  const auto module_path =
      std::filesystem::path(tools::get_module_path(hInstance));

  const auto parent_path = module_path.parent_path();

  initialize_logger(parent_path);

  logger::log("Logger initialized in %s", parent_path.string().c_str());

  const auto config_path = parent_path / "config.json";

  logger::log("Config file located at %s", config_path.string().c_str());

  parse_tosu_env(parent_path);

  ConfigManager::get_instance(config_path.string().c_str());

  const auto cef_path = parent_path / "libcef.dll";

  LoadLibraryEx(cef_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  const auto last_error = GetLastError();
  if (last_error != 0) {
    logger::log("Unable to load libcef.dll (error code: %d)", last_error);
    return;
  }

  if (const auto status = MH_Initialize() != MH_OK) {
    logger::log("MH_Initialize() failed (status == %d)", status);

    return;
  }

  if (const auto status =
          MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers",
                           reinterpret_cast<void*>(swap_buffers_hk),
                           &o_swap_buffers) != MH_OK) {
    logger::log("MH_CreateHookApi() failed (status == %d)", status);

    return;
  }

  if (const auto status = MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
    logger::log("MH_EnableHook() failed (status == %d)", status);

    return;
  }

  logger::log("Starting CEF initialization");

  std::thread{initialize_cef_dll, hInstance}.detach();
}

int32_t __stdcall DllMain(HINSTANCE hInstance, uint32_t reason, uintptr_t) {
  if (reason == DLL_PROCESS_ATTACH) {
    std::thread{main_thread, hInstance}.detach();
  }
  return true;
}
#endif
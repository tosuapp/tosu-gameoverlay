#include <windows.h>
#include <string>

namespace tools {

std::wstring get_module_path(HINSTANCE hInstance) {
  wchar_t module_path[MAX_PATH];
  if (GetModuleFileName(hInstance, module_path, sizeof(module_path)) == 0) {
    printf("GetModuleFileName failed, error = %d\n",
           static_cast<int32_t>(GetLastError()));
    exit(1);
  }

  return module_path;
}

}  // namespace tools
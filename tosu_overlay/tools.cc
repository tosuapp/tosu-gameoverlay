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

uint32_t minmax(uint32_t value, uint32_t minValue, uint32_t maxValue) {
    if (value < minValue) {
        return minValue; // Return minValue if value is less than minValue
    } else if (value > maxValue) {
        return maxValue; // Return maxValue if value is greater than maxValue
    }
    return value; // Return the original value if it's within range
}

}  // namespace tools
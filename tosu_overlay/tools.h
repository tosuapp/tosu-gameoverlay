#pragma once

#include <Windows.h>
#include <string>

namespace tools {
    std::wstring get_module_path(HINSTANCE hInstance);
}
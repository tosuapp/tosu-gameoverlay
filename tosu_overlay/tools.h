#pragma once

#include <Windows.h>
#include <string>

namespace tools {
    std::wstring get_module_path(HINSTANCE hInstance);

    uint32_t minmax(uint32_t value, uint32_t minValue, uint32_t maxValue);
}
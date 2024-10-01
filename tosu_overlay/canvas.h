#pragma once

#include <Windows.h>
#include <cstdint>

namespace canvas {

void create(int32_t width, int32_t height);
void set_data(const void* data);
void draw(HDC hdc);

}  // namespace canvas
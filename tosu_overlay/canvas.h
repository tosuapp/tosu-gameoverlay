#pragma once

#include <Windows.h>
#include <cstdint>

namespace canvas {

void create(int32_t width, int32_t height);
// void set_data(const void* data); // Will be removed
void draw(HDC hdc);

POINT get_render_size();

// Called by CEF thread (OnPaint) to get a direct pointer to a mapped PBO
void* get_direct_paint_buffer(int width, int height);
// Called by CEF thread (OnPaint) after it has finished writing to the PBO
void notify_paint_complete();

}  // namespace canvas
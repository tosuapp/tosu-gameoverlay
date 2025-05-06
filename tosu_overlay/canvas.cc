#include <tosu_overlay/canvas.h>
#include <tosu_overlay/input.h>
#include <tosu_overlay/tosu_overlay_handler.h>
#include <mutex>
#include <atomic> // Added for std::atomic

#include <glad/glad.h>

namespace {

struct GLStateBackup {
  GLint last_active_texture;
  GLint last_program;
  GLint last_texture;
  GLint last_array_buffer;
  GLint last_element_array_buffer;
  GLint last_vertex_array;
  GLint last_polygon_mode[2];
  GLint last_viewport[4];
  GLint last_scissor_box[4];
  GLint last_blend_src_rgb;
  GLint last_blend_dst_rgb;
  GLint last_blend_src_alpha;
  GLint last_blend_dst_alpha;
  GLint last_blend_equation_rgb;
  GLint last_blend_equation_alpha;
  GLboolean last_enable_blend;
  GLboolean last_enable_cull_face;
  GLboolean last_enable_depth_test;
  GLboolean last_enable_scissor_test;

  void backup() {
    glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    glGetIntegerv(GL_VIEWPORT, last_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    glGetIntegerv(GL_BLEND_SRC_RGB, &last_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &last_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
    last_enable_blend = glIsEnabled(GL_BLEND);
    last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
  }

  void restore() {
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb,
                        last_blend_src_alpha, last_blend_dst_alpha);

    // Restore enable/disable states
    if (last_enable_blend)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);

    if (last_enable_cull_face)
      glEnable(GL_CULL_FACE);
    else
      glDisable(GL_CULL_FACE);

    if (last_enable_depth_test)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);

    if (last_enable_scissor_test)
      glEnable(GL_SCISSOR_TEST);
    else
      glDisable(GL_SCISSOR_TEST);

    glPolygonMode(GL_FRONT_AND_BACK, last_polygon_mode[0]);
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2],
               last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2],
              last_scissor_box[3]);
  }
};

GLuint texture = 0;
GLuint program = 0;

const int NUM_PBOS = 2;    // Use two PBOs for ping-pong buffering
GLuint pboIds[NUM_PBOS] = {0};
int pbo_cef_idx = 0;     // PBO index for CEF to write to
int pbo_gl_idx = 1;      // PBO index for OpenGL to upload from

// uint8_t* render_data = nullptr; // Removed: CEF will write directly to PBO
POINT render_size;

std::mutex canvas_create_mutex; // Mutex for canvas::create operations

// Atomics for synchronization between CEF thread and GL thread
std::atomic<void*> current_mapped_buffer_for_cef{nullptr};
std::atomic<bool> cef_buffer_is_ready{false}; // True if a PBO is mapped and ready for CEF
std::atomic<bool> cef_has_painted{false};     // True if CEF has finished painting to the PBO

GLuint vao = 0;
GLuint vbo = 0;
GLint tex_location = -1;

POINT get_window_size(HDC hdc) {
  HWND window = WindowFromDC(hdc);

  RECT rect;
  GetClientRect(window, &rect);

  return {rect.right - rect.left, rect.bottom - rect.top};
}

void create_pbos() {
  // If PBOs already exist, delete them first
  if (pboIds[0] != 0) {
    glDeleteBuffers(NUM_PBOS, pboIds);
  }
  glGenBuffers(NUM_PBOS, pboIds);  // Create PBOs

  for (int i = 0; i < NUM_PBOS; ++i) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[i]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, render_size.x * render_size.y * 4,
                 nullptr, GL_STREAM_DRAW);
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);  // Unbind PBO
  pbo_cef_idx = 0;
  pbo_gl_idx = 1;
}

void try_update_texture() {
  // This function is called on the GL thread.
  // It checks if CEF has painted to its PBO, and if so, uploads it.
  // Then, it maps the next PBO for CEF.

  if (cef_has_painted.load(std::memory_order_acquire)) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[pbo_cef_idx]);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // Unmap the buffer CEF wrote to
    
    // The buffer is no longer mapped for CEF
    current_mapped_buffer_for_cef.store(nullptr, std::memory_order_release);
    // cef_buffer_is_ready will be set to false before mapping the new one,
    // or if mapping fails. For now, CEF cannot use this specific PBO.

    // Upload data from the PBO that CEF just finished with
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_size.x, render_size.y, GL_BGRA,
                    GL_UNSIGNED_BYTE, 0); // Offset is 0 because PBO is bound

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    cef_has_painted.store(false, std::memory_order_release); // Reset for next paint cycle

    // Swap PBOs: the one just uploaded from becomes the next one for CEF to write to
    std::swap(pbo_cef_idx, pbo_gl_idx);
    
    // Signal that CEF does not have a ready buffer, as we just swapped and unmapped.
    // The next block will attempt to map the new pbo_cef_idx.
    cef_buffer_is_ready.store(false, std::memory_order_release);
  }

  // If CEF doesn't have a buffer ready (or we just processed one), map the current pbo_cef_idx for it
  if (!cef_buffer_is_ready.load(std::memory_order_acquire)) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[pbo_cef_idx]);
    // Map the PBO for writing by CEF. GL_MAP_INVALIDATE_BUFFER_BIT is a hint that previous contents can be discarded.
    void* pbo_memory = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                       render_size.x * render_size.y * 4,
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    
    if (pbo_memory) {
      current_mapped_buffer_for_cef.store(pbo_memory, std::memory_order_release);
      cef_buffer_is_ready.store(true, std::memory_order_release);
    } else {
      // Handle mapping failure, though ideally this shouldn't happen often with GL_STREAM_DRAW PBOs
      current_mapped_buffer_for_cef.store(nullptr, std::memory_order_release);
      cef_buffer_is_ready.store(false, std::memory_order_release);
      // TODO: Log an error or handle this case more robustly
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // Unbind PBO from GL_PIXEL_UNPACK_BUFFER target
  }
}

void create_vertex_buffer() {
  // Create and bind VAO
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // Create and bind VBO
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  // Vertex data: position (x,y) and texture coordinates (u,v)
  float vertices[] = {// pos      // tex
                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                      1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};

  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // Texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

const char* v_shader_src = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    uniform vec2 screenSize;

    void main() {
        vec2 pos = aPos * screenSize;
        gl_Position = vec4(pos.x / screenSize.x * 2.0 - 1.0, 
                          1.0 - pos.y / screenSize.y * 2.0, 0.0, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* f_shader_src = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D tex_sampler;

    void main() {
        FragColor = texture(tex_sampler, TexCoord);
        if (FragColor.a < 0.003) 
            discard;
    }
)";

}  // namespace

POINT canvas::get_render_size() {
  return render_size;
}

// Called by CEF thread (OnPaint) to get a direct pointer to a mapped PBO
void* canvas::get_direct_paint_buffer(int width, int height) {
  if (width != render_size.x || height != render_size.y) {
    // Signal to OnPaint that a resize is needed.
    // OnPaint should then call browser->GetHost()->WasResized(),
    // which will eventually lead to canvas::create being called on the GL thread.
    return nullptr;
  }

  if (cef_buffer_is_ready.load(std::memory_order_acquire)) {
    return current_mapped_buffer_for_cef.load(std::memory_order_acquire);
  }
  return nullptr; // Buffer not ready or not mapped
}

// Called by CEF thread (OnPaint) after it has finished writing to the PBO
void canvas::notify_paint_complete() {
  cef_has_painted.store(true, std::memory_order_release);
}

void canvas::create(int32_t width, int32_t height) {
  std::lock_guard<std::mutex> lock(canvas_create_mutex);

  render_size.x = width;
  render_size.y = height;

  // If a PBO was mapped for CEF, unmap it before recreating PBOs
  if (current_mapped_buffer_for_cef.load(std::memory_order_acquire) != nullptr) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[pbo_cef_idx]);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }
  current_mapped_buffer_for_cef.store(nullptr, std::memory_order_release);
  cef_buffer_is_ready.store(false, std::memory_order_release);
  cef_has_painted.store(false, std::memory_order_release);

  // render_data is removed, no need to delete or reallocate it.

  GLint texture2d;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d);

  int32_t alignment = 0;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render_size.x, render_size.y, 0,
               GL_BGRA, GL_UNSIGNED_BYTE, nullptr); // Pass nullptr for data initially
  glBindTexture(GL_TEXTURE_2D, texture2d);

  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

  auto v_shader = glCreateShader(GL_VERTEX_SHADER);
  auto f_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(v_shader, 1, &v_shader_src, 0);
  glShaderSource(f_shader, 1, &f_shader_src, 0);
  glCompileShader(v_shader);
  glCompileShader(f_shader);

  program = glCreateProgram();
  glAttachShader(program, v_shader);
  glAttachShader(program, f_shader);
  glLinkProgram(program);

  // Get uniform locations
  glUseProgram(program);
  tex_location = glGetUniformLocation(program, "tex_sampler");
  GLint screen_size_location = glGetUniformLocation(program, "screenSize");
  glUniform2f(screen_size_location, (float)width, (float)height);
  glUseProgram(0);

  glDeleteShader(v_shader);
  glDeleteShader(f_shader);

  create_vertex_buffer();
  create_pbos();
}

void canvas::draw(HDC hdc) {
  auto window_size = get_window_size(hdc);
  if (window_size.x == 0 || window_size.y == 0) {
    return;
  }

  if (window_size.x != render_size.x || window_size.y != render_size.y) {
    create(window_size.x, window_size.y);
  }

  try_update_texture();

  GLStateBackup state;
  state.backup();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  glUseProgram(program);
  glBindVertexArray(vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(tex_location, 0);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glBindVertexArray(0);
  glUseProgram(0);

  state.restore();
}
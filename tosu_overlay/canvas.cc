#include <tosu_overlay/canvas.h>
#include <tosu_overlay/input.h>
#include <tosu_overlay/tosu_overlay_handler.h>
#include <mutex>

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

GLuint pboIds[4];    // Buffered PBOs
int currentPBO = 0;  // To track the active PBO

uint8_t* render_data;
POINT render_size;

std::mutex mutex;

bool update_pending = true;

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
  glGenBuffers(4, pboIds);  // Create two PBOs

  for (int i = 0; i < 4; ++i) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[i]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, render_size.x * render_size.y * 4,
                 nullptr, GL_STREAM_DRAW);
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);  // Unbind PBO
}

void try_update_texture() {
  std::lock_guard<std::mutex> lock(mutex);

  if (!update_pending) {
    return;
  }

  update_pending = false;

  // Double-buffered PBO: bind the current PBO for asynchronous update
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[currentPBO]);

  // Map the PBO so we can write data to it
  void* pboMemory = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  if (pboMemory) {
    memcpy(pboMemory, render_data,
           render_size.x * render_size.y * 4);  // Copy render data to PBO
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);      // Unmap buffer after copying
  }

  // Bind the texture and perform the texture update using the PBO
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_size.x, render_size.y, GL_BGRA,
                  GL_UNSIGNED_BYTE, 0);

  // Switch to the other PBO for the next frame
  currentPBO = (currentPBO + 1) % 4;

  // Unbind PBO and texture after the update
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
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

void canvas::set_data(const void* data) {
  std::lock_guard<std::mutex> lock(mutex);

  if (!render_data) {
    return;
  }

  memcpy(render_data, data, render_size.x * render_size.y * 4);

  update_pending = true;
}

void canvas::create(int32_t width, int32_t height) {
  std::lock_guard<std::mutex> lock(mutex);

  render_size.x = width;
  render_size.y = height;

  render_data = new uint8_t[width * height * 4];
  ZeroMemory(render_data, width * height * 4);

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
               GL_BGRA, GL_UNSIGNED_BYTE, render_data);
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
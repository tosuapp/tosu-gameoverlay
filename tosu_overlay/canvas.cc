#include <tosu_overlay/canvas.h>
#include <mutex>

#include <glad/glad.h>

namespace {

GLuint texture = 0;
GLuint program = 0;

GLuint pboIds[4];  // Buffered PBOs
int currentPBO = 0; // To track the active PBO

uint8_t* render_data;
POINT render_size;

std::mutex mutex;

bool update_pending = true;

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
        glBufferData(GL_PIXEL_UNPACK_BUFFER, render_size.x * render_size.y * 4, nullptr, GL_STREAM_DRAW);
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
      memcpy(pboMemory, render_data, render_size.x * render_size.y * 4);  // Copy render data to PBO
      glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);  // Unmap buffer after copying
  }

  // Bind the texture and perform the texture update using the PBO
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_size.x, render_size.y, GL_BGRA, GL_UNSIGNED_BYTE, 0);

  // Switch to the other PBO for the next frame
  currentPBO = (currentPBO + 1) % 4;

  // Unbind PBO and texture after the update
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

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

  auto v_shader_src =
      "void main(void)"
      "{"
      "	gl_TexCoord[0] = gl_MultiTexCoord0;"
      "	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
      "}";

  auto f_shader_src =
      "uniform sampler2D tex_sampler;"
      "void main(void)"
      "{"
      "	gl_FragColor = texture2D(tex_sampler,gl_TexCoord[0].st);"
      " if (gl_FragColor.a < 0.1) discard;"
      "}";

  glShaderSource(v_shader, 1, &v_shader_src, 0);
  glShaderSource(f_shader, 1, &f_shader_src, 0);
  glCompileShader(v_shader);
  glCompileShader(f_shader);

  program = glCreateProgram();
  glAttachShader(program, v_shader);
  glAttachShader(program, f_shader);
  glLinkProgram(program);

  glDeleteShader(v_shader);
  glDeleteShader(f_shader);

  create_pbos();
}

void canvas::draw(HDC hdc) {
  auto window_size = get_window_size(hdc);
  if (window_size.x == 0 || window_size.y == 0) {
    return;
  }

  // Avoid unnecessary recreation
  if (window_size.x != render_size.x || window_size.y != render_size.y) {
    create(window_size.x, window_size.y);  // Texture reallocation only when needed
  }

  try_update_texture();

  GLint prev_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
  glUseProgram(program);  // Bind shader program

  // Save the current viewport
  GLint prev_viewport[4];
  glGetIntegerv(GL_VIEWPORT, prev_viewport);

  // Set your custom viewport if needed
  glViewport(0, 0, render_size.x, render_size.y);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, (double)render_size.x, (double)render_size.y, 0.0, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);

  // Enable texture and setup blending only once
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindTexture(GL_TEXTURE_2D, texture);  // Bind the texture

  glBegin(GL_TRIANGLE_STRIP);

  glTexCoord2i(0, 0); glVertex3i(0, 0, 0);
  glTexCoord2i(0, 1); glVertex3i(0, render_size.y, 0);
  glTexCoord2i(1, 0); glVertex3i(render_size.x, 0, 0);
  glTexCoord2i(1, 1); glVertex3i(render_size.x, render_size.y, 0);

  glEnd();

  glBindTexture(GL_TEXTURE_2D, 0);

  glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

  glUseProgram(prev_program);  // Restore previous program
}
/**
* The MIT License (MIT)
*
* Copyright (c) 2019-2020 Vincent Davis Jr.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#ifndef SIMPLE_EXAMPLE_H
#define SIMPLE_EXAMPLE_H

#define LUCUR_VKCOMP_API
#define LUCUR_MATH_API
#define LUCUR_WAYLAND_API
#define LUCUR_WAYLAND_CLIENT_API
#define LUCUR_SPIRV_API
#define LUCUR_CLOCK_API
#define CLOCK_MONOTONIC
#include <wlu/lucurious.h>

typedef struct _vertex_2D {
  vec2 pos;
  vec3 color;
} vertex_2D;

#define FREEME(app,wc) \
  do { \
    if (app) wlu_freeup_vk(app); \
    if (wc) wlu_freeup_wc(wc); \
    wlu_release_blocks(); \
  } while(0);

#define check_err(err,app,wc,shader) \
  do { \
    if (!shader && err) wlu_vk_destroy(WLU_DESTROY_VK_SHADER, app, shader); \
    if (err) { FREEME(app, wc) exit(-1); } \
  } while(0);

const char *device_extensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const char *instance_extensions[] = {
  VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
  VK_KHR_SURFACE_EXTENSION_NAME,
  VK_KHR_DISPLAY_EXTENSION_NAME
};

const char spin_square_frag_text[] =
  "#version 450\n"
  "#extension GL_ARB_separate_shader_objects : enable\n"
  "layout(location = 0) in vec3 v_Color;\n"
  "layout(location = 0) out vec4 o_Color;\n"
  "void main() { o_Color = vec4(v_Color, 1.0); }";

const char spin_square_vert_text[] =
  "#version 450\n"
  "#extension GL_ARB_separate_shader_objects : enable\n"
  "#extension GL_ARB_shading_language_420pack : enable\n"
  "layout(set = 0, binding = 0) uniform UniformBufferObject {\n"
  "   mat4 model;\n"
  "   mat4 view;\n"
  "   mat4 proj;\n"
  "} ubo;\n"
  "layout(location = 0) in vec2 i_Position;\n"
  "layout(location = 1) in vec3 i_Color;\n"
  "layout(location = 0) out vec3 v_Color;\n"
  "void main() {\n"
  "   gl_Position = ubo.proj * ubo.view * ubo.model * vec4(i_Position, 0.0, 1.0);\n"
  "   v_Color = i_Color;\n"
  "}";

vec3 spin_eye = {2.0f, 2.0f, 2.0f};
vec3 spin_center = {0.0f, 0.0f, 0.0f};
vec3 spin_up = {0.0f, 0.0f, 1.0f};
uint16_t indices[6] = {0, 1, 2, 2, 3, 0};

#endif

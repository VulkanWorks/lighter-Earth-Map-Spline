#version 460 core

#if defined(TARGET_OPENGL)
#define MIN_DEPTH -1.0f

#elif defined(TARGET_VULKAN)
#define MIN_DEPTH  0.0f

#else
#error Unrecognized target

#endif  // TARGET_OPENGL || TARGET_VULKAN

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_tex_coord;

layout(location = 0) out vec2 tex_coord;

void main() {
  gl_Position = vec4(in_pos, MIN_DEPTH, 1.0);
  tex_coord = in_tex_coord;
}

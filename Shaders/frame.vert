#version 440
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 o_uv;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
};

void main() {
    o_uv = a_uv;
    gl_Position = mvp * vec4(a_position, 0.0, 1.0);
}

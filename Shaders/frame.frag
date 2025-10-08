#version 440
layout(location = 0) in vec2 o_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D uTex;

void main() {
    vec4 texFragment = texture(uTex, o_uv);
    fragColor = texFragment;
}

#version 440
layout(location = 0) in vec2 o_uv;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(o_uv, 1.0, 1.0);
}

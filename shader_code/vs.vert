#version 450

layout(push_constant) uniform pushConstants {
    layout(row_major)mat4x4 viewProj;
} pc;

layout(location=0) in vec3 in_posL;
layout(location=1) in vec3 in_normalL;
layout(location=2) in vec2 in_uv;

layout(location=0) out vec3 out_normal;
layout(location=1) out vec2 out_uv;

void main()
{
    out_uv = in_uv;
    out_normal = in_normalL;

    gl_Position = vec4(in_posL, 1.0f) * pc.viewProj;

    gl_Position.y *= -1;
}

#version 450

layout(set=0, binding=1) uniform sampler2D img;

layout(location=0) in vec3 in_normal;
layout(location=1) in vec2 in_uv;

layout(location=0) out uvec4 out_col;
layout(location=1) out vec4 swp_col;

void main()
{
    vec4 col = texture(img, in_uv);
    //vec4 col = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    col *= dot(normalize(in_normal), vec3(1.0f, 1.0f, 0.0f));
    col += vec4(0.2f, 0.2f, 0.2f, 0.0f);

    swp_col = col;
    out_col = uvec4(uint(col.x*255.0f), uint(col.y*255.0f), uint(col.z*255.0f), 255);
}

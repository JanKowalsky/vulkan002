#version 450

layout(set=0, binding=1) uniform sampler2D img;

layout(location=0) in vec2 tex_coord;

layout(location=0) out uvec4 out_col;
layout(location=1) out vec4 swp_col;

void main()
{
	vec4 col = texture(img, tex_coord);
	swp_col = col;
	out_col = uvec4(uint(col.x*255.0f), uint(col.y*255.0f), uint(col.z*255.0f), 255);//uint(col.w*255.0f));
	
	//swp_col = vec4(1,0,0,0);
	//out_col = uvec4(255,0,0,0);
}
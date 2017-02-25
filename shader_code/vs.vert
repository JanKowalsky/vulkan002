#version 450

const float delta = 1.0f;

layout(push_constant) uniform pushConstants {
    layout(row_major)mat4x4 viewProj;
	float dt;
	float speed;
	uint res_x;
	uint res_y;
	vec3 eyePosW;
	float p0; //padding
	vec3 curDirNW;
} pc;

layout(set=0, binding=0, rgba32f) uniform imageBuffer vPos;

layout(location=0) out vec2 tex_coord;

void main()
{
	float rX = float(pc.res_x);
	float rY = float(pc.res_y);
	
	uvec2 coord = uvec2(gl_VertexIndex % int(pc.res_x), gl_VertexIndex / int(pc.res_x));
	tex_coord = vec2(float(coord.x) / rX, float(coord.y) / rY);
	
	vec3 currPos = imageLoad(vPos, gl_VertexIndex).xyz;
	vec3 destPos = vec3(-rX/2 + float(coord.x)*delta, rY/2 - float(coord.y)*delta, 0);
	
	//vec3 xx = cross(pc.eyePosW - currPos, pc.curDirNW);
	//float cur_dist = length(xx);
	//xx = normalize(xx);
	//if(cur_dist <= 100.0f)
	//	currPos += xx*100.0f;
	
	vec3 eye_to_ver = pc.eyePosW - currPos;
	vec3 proj = dot(eye_to_ver, pc.curDirNW) * pc.curDirNW;
	vec3 v = eye_to_ver - proj;
	float lv = length(v);
	if(lv <= 100.0f)
	{
		v /= lv;
		currPos -= v*100.0f;
	}
	
	vec3 dir = destPos - currPos;
	float dist = length(dir);
	
	if(dist >= pc.speed*pc.dt)
	{
		dir /= dist;
		currPos +=  dir*pc.speed*pc.dt;
		imageStore(vPos, gl_VertexIndex, vec4(currPos, 0));
	}
	else if (dist >= 0.001)
	{
		currPos = destPos;
		imageStore(vPos, gl_VertexIndex, vec4(currPos, 0));
	}
	
	gl_Position = vec4(currPos, 1.0f) * pc.viewProj;
	gl_Position.y *= -1;
}
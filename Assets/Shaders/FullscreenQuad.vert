#version 460

const vec3 triangle[] =
{
	vec3(1.0,3.0,0.0),
	vec3(1.0,-1.0,0.0),
	vec3(-3.0,-1.0,0.0)
};

const vec2 uvs[] =
{
	vec2(1.0,-1.0),
	vec2(1.0,1.0),
	vec2(-1.0,1.0)
};

layout(location = 0 ) out vec2 uv;

void main()
{
	uv = uvs[gl_VertexIndex].xy;
	gl_Position = vec4(triangle[gl_VertexIndex], 1.0);
}
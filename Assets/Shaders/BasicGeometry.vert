#version 460
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0 ) out vec2 uv;
layout(location = 1 ) out vec3 normal;

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 uv0;
};

layout(scalar, set=0, binding=0) readonly buffer globalGeometryBufferBlock
{
	Vertex globalGeometryBuffer[];
};
layout(scalar, set=0, binding=1) readonly buffer globalGeometryIndexBufferBlock
{
	int globalGeometryIndexBuffer[];
};

layout(push_constant) uniform constantsBlock 
{
	layout(offset = 32)
	mat4 viewProjection;
	mat4 model;
} constants;


void main()
{
	int index = globalGeometryIndexBuffer[gl_VertexIndex];
	vec3 position = globalGeometryBuffer[index].position * vec3(1.0f, -1.0f, 1.0f);
	uv = globalGeometryBuffer[index].uv0;
	normal = globalGeometryBuffer[index].normal;
	gl_Position = constants.viewProjection * constants.model * vec4(position, 1.0f);
}
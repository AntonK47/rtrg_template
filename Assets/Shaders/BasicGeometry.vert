#version 460
 #extension GL_EXT_scalar_block_layout : enable

layout(location = 0 ) out vec2 uv;

struct Vertex
{
	vec3 position;
	vec2 uv0;
};

layout(scalar,set=0, binding=0) readonly buffer globalGeometryBufferBlock
{
	Vertex globalGeometryBuffer[];
};
layout(scalar,set=0, binding=1) readonly buffer globalGeometryIndexBufferBlock
{
	int globalGeometryIndexBuffer[];
};

void main()
{
	int index = globalGeometryIndexBuffer[gl_VertexIndex];
	vec3 position = globalGeometryBuffer[index].position * vec3(1.0f, -1.0f, 1.0f);
	uv = globalGeometryBuffer[index].uv0;

	gl_Position = vec4(position * 0.3f + vec3(0.0f,0.0f,0.5f), 1.0);
}
#version 460
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0 ) out vec2 uv;
layout(location = 1 ) out vec3 normal;

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 uv0;
	uint jointIndicies;
	vec4 jointWeights;
};

layout(scalar, set=0, binding=0) readonly buffer globalGeometryBufferBlock
{
	Vertex globalGeometryBuffer[];
};
layout(scalar, set=0, binding=1) readonly buffer globalGeometryIndexBufferBlock
{
	int globalGeometryIndexBuffer[];
};

layout(set=1, binding=0) readonly uniform JointMatricies
{
	mat4 jointMatricies[256];
};

layout(push_constant) uniform constantsBlock 
{
	layout(offset = 32)
	mat4 viewProjection;
	mat4 view;
	mat4 model;
} constants;



void main()
{
	int index = globalGeometryIndexBuffer[gl_VertexIndex];
	vec3 position = globalGeometryBuffer[index].position;// * vec3(1.0f, -1.0f, 1.0f);
	uv = globalGeometryBuffer[index].uv0;
	

	uint jointIndicies = globalGeometryBuffer[index].jointIndicies;
	uint i0 = jointIndicies & 255;
	uint i1 = (jointIndicies >> 8) & 255;
	uint i2 = (jointIndicies >> 16) & 255;
	uint i3 = (jointIndicies >> 24) & 255;

	vec4 jointWeights = globalGeometryBuffer[index].jointWeights;

	mat4 skinning = jointMatricies[int(i0)] * jointWeights.x 
		+ jointMatricies[int(i1)] * jointWeights.y + jointMatricies[int(i2)] * jointWeights.z + jointMatricies[int(i3)] * jointWeights.w;

	gl_Position = constants.viewProjection * constants.model * skinning * vec4(position, 1.0f);

	vec3 vertexNormal = globalGeometryBuffer[index].normal;
	normal = normalize(transpose(inverse(mat3(constants.view * constants.model * skinning))) * vertexNormal);
}
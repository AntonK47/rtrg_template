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
	uint globalGeometryBuffer[];
};
layout(scalar, set=0, binding=1) readonly buffer globalGeometryIndexBufferBlock
{
	int globalGeometryIndexBuffer[];
};

layout(set=1, binding=0) readonly uniform JointMatricies
{
	mat4 jointMatricies[256];
};


struct SubMesh
{
	uint indexBase;
	uint vertexBase;
	uint vertexStride;
};

layout(set=0, binding = 2) readonly buffer registeredSubMeshes
{
	SubMesh subMeshes[1024];
};

layout(push_constant) uniform constantsBlock 
{
	layout(offset = 32)
	mat4 viewProjection;
	mat4 view;
	mat4 model;
} constants;

Vertex decode(in uint vertexOffset)
{
	vec4 v1 = unpackSnorm4x8(globalGeometryBuffer[vertexOffset]);
	vec4 v2 = unpackSnorm4x8(globalGeometryBuffer[vertexOffset + 1]);
	

	vec3 position = v1.xyz;
	vec3 normal = vec3(v1.w, v2.xy);
	vec2 uv0 = v2.zw;
	uint jointIndicies = globalGeometryBuffer[vertexOffset + 2];
	vec4 jointWeights = unpackSnorm4x8(globalGeometryBuffer[vertexOffset + 3]);

	Vertex v;
	v.position = position;
	v.normal = normal;
	v.uv0 = uv0;
	v.jointIndicies = jointIndicies;
	v.jointWeights = jointWeights;
	return v;
}

void main()
{
	SubMesh subMesh = subMeshes[gl_InstanceIndex];

	int index = globalGeometryIndexBuffer[gl_VertexIndex + int(subMesh.indexBase)];
	uint vertexOffset = subMesh.vertexBase + subMesh.vertexStride * index;

	Vertex vertex = decode(vertexOffset);

	vec3 position = vertex.position;
	uv = vertex.uv0;

	uint jointIndicies = vertex.jointIndicies;
	uint i0 = jointIndicies & 255;
	uint i1 = (jointIndicies >> 8) & 255;
	uint i2 = (jointIndicies >> 16) & 255;
	uint i3 = (jointIndicies >> 24) & 255;

	vec4 jointWeights = vertex.jointWeights;

	mat4 skinning = jointMatricies[int(i0)] * jointWeights.x 
		+ jointMatricies[int(i1)] * jointWeights.y + jointMatricies[int(i2)] * jointWeights.z + jointMatricies[int(i3)] * jointWeights.w;

	gl_Position = constants.viewProjection * constants.model * skinning * vec4(position, 1.0f);

	vec3 vertexNormal = vertex.normal;
	normal = normalize(transpose(inverse(mat3(constants.view * constants.model * skinning))) * vertexNormal);
}
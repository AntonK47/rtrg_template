#version 460
#extension GL_EXT_scalar_block_layout : enable

#extension GL_GOOGLE_include_directive : require
#include "Common/Skinning.Library.glsl"

#include "UnifiedGeometryBuffer.Library.glsl"

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 positionWS;
layout(location = 3) out vec3 viewPositionWS;

struct SkinnedVertex
{
	vec3 position;
	vec3 normal;
	vec2 uv0;
	uint jointIndicies;
	vec4 jointWeights;
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
	vec3 viewPositionWS;
} constants;


SkinnedVertex decode(uint baseOffset, uint vertexOffset)
{
	vec3 position = UNIFIED_GEOMETRY_BUFFER_GetVec3(baseOffset, vertexOffset);
	vec3 normal = UNIFIED_GEOMETRY_BUFFER_GetVec3(baseOffset, vertexOffset + 3);
	vec2 uv0 = UNIFIED_GEOMETRY_BUFFER_GetVec2(baseOffset, vertexOffset + 6);
	uint jointIndicies = UNIFIED_GEOMETRY_BUFFER_GetUint(baseOffset, vertexOffset + 8);
	vec4 jointWeights = UNIFIED_GEOMETRY_BUFFER_GetVec4(baseOffset, vertexOffset + 9);


	SkinnedVertex v;
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
	uint vertexByteOffset = 13 * index;

	SkinnedVertex vertex = decode(subMesh.vertexBase * 13, vertexByteOffset);
	//TODO: remove hardcoded byte stide

	vec3 position = vertex.position;
	uv = vertex.uv0;

	ivec4 jointIndicies = decodeJointIndicies(vertex.jointIndicies);

	vec4 jointWeights = vertex.jointWeights;

	mat4 skinning = jointMatricies[jointIndicies.x] * jointWeights.x 
		+ jointMatricies[jointIndicies.y] * jointWeights.y + jointMatricies[jointIndicies.z] * jointWeights.z + jointMatricies[jointIndicies.w] * jointWeights.w;

	positionWS = vec3(constants.model * skinning * vec4(position, 1.0f));

	gl_Position = constants.viewProjection * vec4(positionWS,1.0f);

	vec3 vertexNormal = vertex.normal;
	normal = transpose(inverse(mat3(skinning))) * vertexNormal;
	viewPositionWS = constants.viewPositionWS;
}
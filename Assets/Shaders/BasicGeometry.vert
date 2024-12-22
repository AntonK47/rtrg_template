#version 460
#extension GL_EXT_scalar_block_layout : enable

#extension GL_GOOGLE_include_directive : require
#include "Common/Skinning.Library.glsl"

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 positionWS;
layout(location = 3) out vec3 viewPositionWS;

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


/*Vertex decode(in uint vertexOffset)
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
*/
void main()
{
	SubMesh subMesh = subMeshes[gl_InstanceIndex];

	int index = globalGeometryIndexBuffer[gl_VertexIndex + int(subMesh.indexBase)];
	uint vertexOffset = subMesh.vertexBase + subMesh.vertexStride * index;

	
	Vertex vertex = globalGeometryBuffer[subMesh.vertexBase + index];
	//Vertex vertex = decode(vertexOffset);

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
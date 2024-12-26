#ifndef UNIFIED_GEOMETRY_BUFFER_LIBRARY_GLSL
#define UNIFIED_GEOMETRY_BUFFER_LIBRARY_GLSL

#extension GL_GOOGLE_include_directive : require
#include "UnifiedGeometryBuffer.Shared.Common.Library.h"

layout(set=UNIFIED_GEOMETRY_BUFFER_BASE_SET, binding=0) readonly buffer unifiedGeometryBufferBlock
{
	uint unifiedGeometryBuffer[];
};

layout(set=UNIFIED_GEOMETRY_BUFFER_BASE_SET, binding=1) readonly buffer unifiedGeometryBufferLookupTableBlock
{
	uint unifiedGeometryBufferLookupTable[];
};

#define UINT_BYTES_SIZE 2

uint UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(uint virtualPageOffset, uint offset)
{
	return virtualPageOffset + offset;
}

vec3 UNIFIED_GEOMETRY_BUFFER_GetVec3(uint baseOffset,uint byteOffset)
{
	uint uintValueIndex0 = byteOffset;
	uint uintValueIndex1 = uintValueIndex0 + 1;
	uint uintValueIndex2 = uintValueIndex1 + 1;

	uint offset0 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex0);
	uint offset1 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex1);
	uint offset2 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex2);
	uint packedValue0 = unifiedGeometryBuffer[offset0];
	uint packedValue1 = unifiedGeometryBuffer[offset1];
	uint packedValue2 = unifiedGeometryBuffer[offset2];
	float value0 = uintBitsToFloat(packedValue0);
	float value1 = uintBitsToFloat(packedValue1);
	float value2 = uintBitsToFloat(packedValue2);

	vec3 result = vec3(value0, value1, value2);
	return result;
}

vec2 UNIFIED_GEOMETRY_BUFFER_GetVec2(uint baseOffset, uint byteOffset)
{
	uint uintValueIndex0 = byteOffset;
	uint uintValueIndex1 = uintValueIndex0 + 1;


	uint offset0 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex0);
	uint offset1 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex1);
	uint packedValue0 = unifiedGeometryBuffer[offset0];
	uint packedValue1 = unifiedGeometryBuffer[offset1];

	float value0 = uintBitsToFloat(packedValue0);
	float value1 = uintBitsToFloat(packedValue1);

	vec2 result = vec2(value0, value1);
	return result;
}

vec4 UNIFIED_GEOMETRY_BUFFER_GetVec4(uint baseOffset, uint byteOffset)
{
	vec2 value0 = UNIFIED_GEOMETRY_BUFFER_GetVec2(baseOffset, byteOffset);
	vec2 value1 = UNIFIED_GEOMETRY_BUFFER_GetVec2(baseOffset, byteOffset + UINT_BYTES_SIZE);
	return vec4(value0, value1);
}

uint UNIFIED_GEOMETRY_BUFFER_GetUint(uint baseOffset, uint byteOffset)
{
	uint byteOffset0 = byteOffset;
	uint offset0 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, byteOffset0);
	return unifiedGeometryBuffer[offset0];
}

vec4 UNIFIED_GEOMETRY_BUFFER_GetVec4u8(uint baseOffset, uint byteOffset)
{
	uint uintValueIndex0 = byteOffset;
	uint offset0 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex0);
	uint packedValue0 = unifiedGeometryBuffer[offset0];

	vec4 result = unpackUnorm4x8(packedValue0);
	return result;
}

#if 0 //TODO: Half precision, not tested yet 
vec3 UNIFIED_GEOMETRY_BUFFER_GetVec3f16(uint baseOffset,uint vertexByteOffset)
{
	uint uintValueIndex0 = vertexByteOffset / 2;
	uint uintValueIndex1 = uintValueIndex0 + 1;


	uint offset0 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex0);
	uint offset1 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex1);
	uint packedValue0 = unifiedGeometryBuffer[offset0];
	uint packedValue1 = unifiedGeometryBuffer[offset1];
	vec2 value0 = unpackHalf2x16(packedValue0);
	vec2 value1 = unpackHalf2x16(packedValue1);

	vec3 result0 = vec3(value0.x, value0.y, value1.x);
	vec3 result1 = vec3(value0.y, value1.x, value1.y);

	return vertexByteOffset % 2 == 0 ? result0 : result1;
}

vec2 UNIFIED_GEOMETRY_BUFFER_GetVec2f16(uint baseOffset, uint vertexByteOffset)
{
	uint uintValueIndex0 = vertexByteOffset / 2;
	uint uintValueIndex1 = uintValueIndex0 + 1;


	uint offset0 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex0);
	uint offset1 = UNIFIED_GEOMETRY_BUFFER_GetPhysicalByteOffset(baseOffset, uintValueIndex1);
	vec2 value0 = unpackHalf2x16(unifiedGeometryBuffer[offset0]);
	vec2 value1 = unpackHalf2x16(unifiedGeometryBuffer[offset1]);

	vec2 result0 = vec2(value0.x, value0.y);
	vec2 result1 = vec2(value0.y, value1.x);

	return vertexByteOffset % 2 == 0 ? result0 : result1;
}

vec4 UNIFIED_GEOMETRY_BUFFER_GetVec4f16(uint baseOffset, uint vertexByteOffset)
{
	vec2 value0 = UNIFIED_GEOMETRY_BUFFER_GetVec2(baseOffset, vertexByteOffset);
	vec2 value1 = UNIFIED_GEOMETRY_BUFFER_GetVec2(baseOffset, vertexByteOffset + 2);
	return vec4(value0, value1);
}
#endif
#endif
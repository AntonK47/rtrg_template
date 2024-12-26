#ifndef COMMON_LIBRARY_GLSL
#define COMMON_LIBRARY_GLSL

struct TangentFrame
{
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
};

vec3 decodeNormal(uint packedNormal)
{
	uint packedValue = packedNormal;
	float x = uintBitsToFloat(packedValue & uint(0x0000FFFF));
	float y = uintBitsToFloat((packedValue & uint(0xFFFF0000)) >> 16);
	float z = sqrt(1.0 - x * x - y * y);
	return vec3(x, y, z);
}

//source: ray tracing gems 2
float signNotZero(in float k) 
{
	return (k >= 0.0f) ? 1.0f :-1.0f;
}
 
vec2 signNotZero(in vec2 v) 
{
	return vec2(signNotZero(v.x), signNotZero(v.y));
}

vec2 octahedralEncode(vec3 v) 
{
	float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
	vec2 result = v.xy * (1.0f / l1norm);
	if (v.z < 0.0f) 
	{
		result = (1.0f - abs(result.yx)) * signNotZero(result.xy);
	}
	return result;
}

vec3 octahedralDecode(vec2 o) 
{
	vec3 v = vec3(o.x, o.y, 1.0f - abs(o.x)- abs(o.y));
	if (v.z < 0.0f) 
	{
		v.xy = (1.0f- abs(v.yx)) * signNotZero(v.xy);
	}
	return normalize(v);
}

//From 3 BYTE TANGENT FRAMES https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf
TangentFrame decodeTangentFrame(uint packedTangentFrame, uint baseOffsetBitShift)
{
	uint a = (packedTangentFrame >> baseOffsetBitShift) & uint(0xFF);
	uint b = (packedTangentFrame >> (baseOffsetBitShift + 8)) & uint(0xFF);
	uint c = (packedTangentFrame >> (baseOffsetBitShift + 16)) & uint(0xFF);

	float angle = uintBitsToFloat(c);
	vec2 octahedralUV = vec2(uintBitsToFloat(a), uintBitsToFloat(b));
	vec3 normal = octahedralDecode(octahedralUV);

	vec3 naiveTangent = vec3(0.0f, 0.0f, 0.0f);

	if(abs(normal.x) > abs(normal.z))
	{
		naiveTangent.x = -normal.y;
		naiveTangent.y = normal.x;
	}
	else
	{
		naiveTangent.y = -normal.z;
		naiveTangent.z = normal.y;
	}

	vec3 tangent = naiveTangent * cos(angle) + cross(normal, naiveTangent) * sin(angle);

	vec3 bitangent = cross(normal, tangent);

	TangentFrame frame;
	frame.normal = normal;
	frame.tangent = tangent;
	frame.bitangent = bitangent;

	return frame;
}

#endif
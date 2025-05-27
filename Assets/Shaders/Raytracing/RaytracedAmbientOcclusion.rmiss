#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "RaytracedAmbientOcclusion.Shared.h"

layout(location = 0) rayPayloadEXT RayPayload pay;

void main()
{
	pay.hitSky = 1.0f;
}
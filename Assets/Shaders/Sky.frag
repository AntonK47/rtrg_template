//Example from: https://www.shadertoy.com/view/4td3zj
#version 460

#extension GL_GOOGLE_include_directive : require
#include "CheapAtmosphere.Library.glsl"

layout(location = 0) out vec4 outputColor;
layout(location = 0) in vec2 uv;

layout(push_constant) uniform SomeValues { float time; float resolution[2]; vec3 camera_forward; vec3 camera_up; } values;

#define iTime values.time
#define iResolution vec2(values.resolution[0], values.resolution[1])
const vec2 iMouse = vec2(0.0f, 0.0f);

vec3 getSunDirection()
{
  float time = iTime*100.0f;
  return normalize(vec3(sin(time), 1.0, cos(time)));
}

vec3 getAtmosphere(vec3 dir)
{
  return extra_cheap_atmosphere(dir, getSunDirection()) * 0.5;
}

float getSun(vec3 dir)
{
  return pow(max(0.0, dot(dir, getSunDirection())), 720.0) * 210.0;
}

mat3 createRotationMatrixAxisAngle(vec3 axis, float angle) {
  float s = sin(angle);
  float c = cos(angle);
  float oc = 1.0 - c;
  return mat3(
    oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s,
    oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s,
    oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c
  );
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 ray = normalize(vec3(2.*fragCoord - iResolution.xy, iResolution.y));


    vec3 camera_right = normalize(cross(values.camera_forward, values.camera_up));
	mat3 camera = mat3(values.camera_forward, camera_right, values.camera_up);
    ray = camera * ray;



    vec3 color = getAtmosphere(ray) + getSun(ray);
	fragColor = vec4(sqrt(clamp(color, 0., 1.))*0.2f, 1.);
}


void main()
{
	mainImage(outputColor, /*gl_FragCoord.xy*/ uv * iResolution);
}
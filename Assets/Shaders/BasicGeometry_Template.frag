#version 460

layout(location = 0) out vec4 outputColor;
layout(location = 0) in vec2 uv;
layout(location = 1 ) in vec3 normal;

layout(push_constant) uniform SomeValues { float time; float resolution[2]; } values;

#define iTime values.time
#define iResolution vec2(values.resolution[0], values.resolution[1])
const vec2 iMouse = vec2(0.0f, 0.0f);


struct Geometry
{
    vec3 normal;
    vec2 uv;
};

//void surface(in Geometry geometry, out vec4 color)
%%material_evaluation_code%%

void main()
{
    vec3 lightDirection = vec3(1.0f,1.0f,0.0f);
    vec3 n = normalize(normal);
    float a = max(0, dot(lightDirection, n));
    vec3 ambient = vec3(0.3f, 0.1f, 0.1f);

   

    Geometry geometry;
    geometry.normal = n;
    geometry.uv = uv;
     
    vec4 materialColor;
	surface(geometry, materialColor);

    outputColor = vec4(materialColor.xyz * a + ambient, 1.0f);
}
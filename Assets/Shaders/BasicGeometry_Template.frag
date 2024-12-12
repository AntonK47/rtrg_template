#version 460

layout(location = 0) out vec4 outputColor;
layout(location = 0) in vec2 uv;
layout(location = 1 ) in vec3 normal;
layout(location = 2) in vec3 positionWS;
layout(location = 3) in vec3 viewPositionWS;

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
    float radius = 400.0f;
    vec3 pointLightPositionWS = vec3(radius * sin(iTime*2.f),radius*cos(iTime*2.0f), 200.0f);
    vec3 lightDirection = normalize(pointLightPositionWS - positionWS);
    vec3 n = normalize(normal);
    float a = max(0, dot(lightDirection, n));
    vec3 ambient = vec3(0.3f, 0.1f, 0.1f);

   

    Geometry geometry;
    geometry.normal = n;
    geometry.uv = uv;
     
    vec4 materialColor;
	surface(geometry, materialColor);

    vec3 viewDirection = normalize(viewPositionWS - positionWS);

    float specularTerm = pow( max(0.0f, dot(reflect(lightDirection, n), viewDirection)), 15.0f);
    

    outputColor = vec4(materialColor.xyz * a + ambient + specularTerm*vec3(1.0f,1.0f,1.0f), 1.0f);
}
#include <gtest/gtest.h>
#include <string>

#include <Utils.hpp>

// Demonstrate some basic assertions.
TEST(CompilationToSPIRV, VertexAndFragmentShaderCompilationTest)
{
	auto shader01 =
		R"(#version 460 core

			int i;
			; // extraneous semicolon okay
			float f;
			;
			;

			void main()
			{
				bool b1;
				float array[int(mod(float(7.1), float(4.0)))];
				b1 = anyInvocation(b1);
				b1 = allInvocations(b1);
				b1 = allInvocationsEqual(b1);
			};
			;)";

	auto shader02 =
		R"(#version 450

			layout (binding = 1) uniform sampler2D samplerColor;
			layout (binding = 2) uniform sampler2D samplerNormalMap;

			layout (location = 0) in vec3 inNormal;
			layout (location = 1) in vec2 inUV;
			layout (location = 2) in vec3 inColor;
			layout (location = 3) in vec3 inWorldPos;
			layout (location = 4) in vec3 inTangent;

			layout (location = 0) out vec4 outPosition;
			layout (location = 1) out vec4 outNormal;
			layout (location = 2) out vec4 outAlbedo;

			void main() 
			{
				outPosition = vec4(inWorldPos, 1.0);

				// Calculate normal in tangent space
				vec3 N = normalize(inNormal);
				vec3 T = normalize(inTangent);
				vec3 B = cross(N, T);
				mat3 TBN = mat3(T, B, N);
				vec3 tnorm = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));
				outNormal = vec4(tnorm, 1.0);

				outAlbedo = texture(samplerColor, inUV);
			})";


	const auto shaderInfo01 =
		Utils::ShaderInfo{ "main", {}, Utils::ShaderStage::Vertex, Utils::GlslShaderCode{ shader01 } };
	const auto shaderInfo02 =
		Utils::ShaderInfo{ "main", {}, Utils::ShaderStage::Fragment, Utils::GlslShaderCode{ shader02 } };

	Utils::ShaderByteCode code01;
	Utils::ShaderByteCode code02;
	const auto result01 = Utils::CompileToSpirv(shaderInfo01, code01);
	const auto result02 = Utils::CompileToSpirv(shaderInfo02, code02);

	EXPECT_TRUE(result01 == Utils::CompilationResult::Success);
	EXPECT_TRUE(result02 == Utils::CompilationResult::Success);
}
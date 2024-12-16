#pragma once

#include <string>
#include <vector>

namespace Utils
{
	using ShaderByteCode = std::vector<uint32_t>;
	using GlslShaderCode = std::string;

	enum class ShaderStage
	{
		Vertex,
		Fragment,
		Geometry,
		TessellationControl,
		TessellationEvaluation,
		Task,
		Mesh,
		AnyHit,
		ClosestHit,
		Miss,
		RayGeneration,
		Intersection,
	};

	enum class CompilationResult
	{
		Success,
		Failed
	};

	struct ShaderInfo
	{
		std::string entryPoint{ "main" };
		std::vector<std::string> compilationDefines{};
		ShaderStage shaderStage{ ShaderStage::Vertex };
		GlslShaderCode shaderCode{};
		bool enableDebugCompilation{ true };
	};

	CompilationResult CompileToSpirv(const ShaderInfo& info, ShaderByteCode& byteCode);
} // namespace Utils
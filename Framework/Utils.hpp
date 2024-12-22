#pragma once

#include <Core.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glslang/Include/glslang_c_interface.h>

namespace Framework
{
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
			std::string name{ "" };
		};


		struct CompilerOptions
		{
			bool optimize{ false };
			bool stripDebugInfo{ false };
			std::filesystem::path includePath{};
			std::function<void(const char*)> logCallback;
		};

		struct ShaderCompiler final
		{
			ShaderCompiler(CompilerOptions options);
			~ShaderCompiler();

			CompilationResult CompileToSpirv(const ShaderInfo& info, ShaderByteCode& byteCode);

			glslang_spv_options_t spirvOptions;
			std::filesystem::path includePath{};
			std::function<void(const char*)> logCallback;

			struct CachedIncludeValue
			{
				U32 counter{ 0 };
				std::string includeName;
				std::string sourceCode;
			};

			std::unordered_map<std::string, CachedIncludeValue> includesCache;
		};

		CompilationResult CompileToSpirv(const ShaderInfo& info, ShaderByteCode& byteCode);
	} // namespace Utils
} // namespace Framework
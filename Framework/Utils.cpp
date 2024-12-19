#include "Utils.hpp"

#include <cassert>
#include <fstream>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/resource_limits_c.h>
#include <print>


using namespace Framework;
using namespace Framework::Utils;
namespace
{
	EShLanguage MapShaderStage(const Utils::ShaderStage stage)
	{
		switch (stage)
		{
		case Utils::ShaderStage::Vertex:
			return EShLangVertex;
		case Utils::ShaderStage::Fragment:
			return EShLangFragment;
		case Utils::ShaderStage::Geometry:
			return EShLangGeometry;
		case Utils::ShaderStage::TessellationControl:
			return EShLangTessControl;
		case Utils::ShaderStage::TessellationEvaluation:
			return EShLangTessEvaluation;
		case Utils::ShaderStage::Task:
			return EShLangTaskNV; // soon it will change to non vendor specific
								  // version
		case Utils::ShaderStage::Mesh:
			return EShLangMeshNV;
		case Utils::ShaderStage::AnyHit:
			return EShLangAnyHit;
		case Utils::ShaderStage::ClosestHit:
			return EShLangClosestHit;
		case Utils::ShaderStage::Miss:
			return EShLangMiss;
		case Utils::ShaderStage::RayGeneration:
			return EShLangRayGen;
		case Utils::ShaderStage::Intersection:
			return EShLangIntersect;
		}
		return {};
	}

	glsl_include_result_t* includeLocalAndSystem(void* ctx, const char* header_name, const char* includer_name,
												 size_t include_depth)
	{
		Utils::ShaderCompiler* compiler = reinterpret_cast<Utils::ShaderCompiler*>(ctx);
		const auto key = std::string{ header_name };
		if (compiler->includesCache.contains(key))
		{
			compiler->includesCache[key].counter++;
			return new glsl_include_result_t{ .header_name = compiler->includesCache[key].includeName.c_str(),
											  .header_data = compiler->includesCache[key].sourceCode.data(),
											  .header_length = compiler->includesCache[key].sourceCode.size() };
		}

		const auto headerFilePath = compiler->includePath / header_name;

		const auto includeFileFound = std::filesystem::exists(headerFilePath);
		if (includeFileFound)
		{
			auto stream = std::ifstream{ headerFilePath, std::ios::ate };
			const auto size = stream.tellg();
			auto shader = std::string(size, '\0');
			stream.seekg(0);
			stream.read(&shader[0], size);
			const auto charCount = stream.gcount();
			shader = shader.substr(0, charCount);
			compiler->includesCache.insert(std::make_pair(key, ShaderCompiler::CachedIncludeValue{ 1, key, shader }));

			return new glsl_include_result_t{ .header_name = compiler->includesCache[key].includeName.c_str(),
											  .header_data = compiler->includesCache[key].sourceCode.data(),
											  .header_length = compiler->includesCache[key].sourceCode.size() };
		}
		return new glsl_include_result_t{ .header_name = NULL, .header_data = NULL, .header_length = 0 };
	}

	int freeInclude(void* ctx, glsl_include_result_t* result)
	{
		Utils::ShaderCompiler* compiler = reinterpret_cast<Utils::ShaderCompiler*>(ctx);
		const auto key = std::string{ result->header_name };
		assert(compiler->includesCache.contains(key));
		compiler->includesCache[key].counter--;
		if (compiler->includesCache[key].counter == 0)
		{
			compiler->includesCache.erase(key);
		}
		delete result;
		return 0;
	}

} // namespace

Utils::ShaderCompiler::ShaderCompiler(CompilerOptions options) : includePath{ options.includePath }, logCallback{options.logCallback}
{
	const auto optimize = options.optimize;
	const auto stripDebugInfo = options.stripDebugInfo;

	spirvOptions = glslang_spv_options_t{ .generate_debug_info = not stripDebugInfo,
										  .strip_debug_info = stripDebugInfo,
										  .disable_optimizer = not optimize,
										  .optimize_size = false,
										  .disassemble = false,
										  .validate = true,
										  .emit_nonsemantic_shader_debug_info = not stripDebugInfo,
										  .emit_nonsemantic_shader_debug_source = not stripDebugInfo };
	glslang_initialize_process();
}

Framework::Utils::ShaderCompiler::~ShaderCompiler()
{
	glslang_finalize_process();
}

Utils::CompilationResult Utils::ShaderCompiler::CompileToSpirv(const ShaderInfo& info, ShaderByteCode& byteCode)
{
	
	const auto stage = static_cast<glslang_stage_t>(MapShaderStage(info.shaderStage));

	const auto includeCallbacks = glsl_include_callbacks_t{ .include_system = &includeLocalAndSystem,
															.include_local = &includeLocalAndSystem,
															.free_include_result = &freeInclude };

	const glslang_input_t input = { .language = GLSLANG_SOURCE_GLSL,
									.stage = stage,
									.client = GLSLANG_CLIENT_VULKAN,
									.client_version = GLSLANG_TARGET_VULKAN_1_3,
									.target_language = GLSLANG_TARGET_SPV,
									.target_language_version = GLSLANG_TARGET_SPV_1_6,
									.code = info.shaderCode.data(),
									.default_version = 460,
									.default_profile = GLSLANG_NO_PROFILE,
									.force_default_version_and_profile = false,
									.forward_compatible = false,
									.messages = GLSLANG_MSG_DEFAULT_BIT,
									.resource = glslang_default_resource(),
									.callbacks = includeCallbacks,
									.callbacks_ctx = this };

	glslang_shader_t* shader = glslang_shader_create(&input);

	if (!glslang_shader_preprocess(shader, &input))
	{
		logCallback("GLSL preprocessing failed!");
		logCallback(glslang_shader_get_info_log(shader));
		logCallback(glslang_shader_get_info_debug_log(shader));
		logCallback(input.code);
		glslang_shader_delete(shader);
		return CompilationResult::Failed;
	}

	if (!glslang_shader_parse(shader, &input))
	{
		logCallback("GLSL parsing failed!");
		logCallback(glslang_shader_get_info_log(shader));
		logCallback(glslang_shader_get_info_debug_log(shader));
		logCallback(glslang_shader_get_preprocessed_code(shader));
		glslang_shader_delete(shader);
		return CompilationResult::Failed;
	}

	glslang_program_t* program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
	{
		logCallback("GLSL linking failed!");
		logCallback(glslang_program_get_info_log(program));
		logCallback(glslang_program_get_info_debug_log(program));
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return CompilationResult::Failed;
	}


	glslang_program_SPIRV_generate_with_options(program, stage, &spirvOptions);

	byteCode.resize(glslang_program_SPIRV_get_size(program));
	glslang_program_SPIRV_get(program, byteCode.data());

	if (const char* spirvMessages = glslang_program_SPIRV_get_messages(program))
	{
		logCallback(spirvMessages);
	}

	glslang_program_delete(program);
	glslang_shader_delete(shader);

	
	return Utils::CompilationResult::Success;
}

Utils::CompilationResult Utils::CompileToSpirv(const ShaderInfo& info, ShaderByteCode& byteCode)
{
	auto compiler = ShaderCompiler{ CompilerOptions{} };
	return compiler.CompileToSpirv(info, byteCode);
}
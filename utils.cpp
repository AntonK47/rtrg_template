#include "Utils.hpp"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/resource_limits_c.h>
#include <print>

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
} // namespace

Utils::CompilationResult Utils::CompileToSpirv(const ShaderInfo& info, ShaderByteCode& byteCode)
{
	glslang_initialize_process();
	const auto stage = static_cast<glslang_stage_t>(MapShaderStage(info.shaderStage));
	const glslang_input_t input = {
		.language = GLSLANG_SOURCE_GLSL,
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
	};

	glslang_shader_t* shader = glslang_shader_create(&input);

	if (!glslang_shader_preprocess(shader, &input))
	{
		std::println("GLSL preprocessing failed!");
		std::println("{}", glslang_shader_get_info_log(shader));
		std::println("{}", glslang_shader_get_info_debug_log(shader));
		std::println("{}", input.code);
		glslang_shader_delete(shader);
		return CompilationResult::Failed;
	}

	if (!glslang_shader_parse(shader, &input))
	{
		std::println("GLSL parsing failed!");
		std::println("{}", glslang_shader_get_info_log(shader));
		std::println("{}", glslang_shader_get_info_debug_log(shader));
		std::println("{}", glslang_shader_get_preprocessed_code(shader));
		glslang_shader_delete(shader);
		return CompilationResult::Failed;
	}

	glslang_program_t* program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
	{
		std::println("GLSL linking failed!");
		std::println("{}", glslang_program_get_info_log(program));
		std::println("{}", glslang_program_get_info_debug_log(program));
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return CompilationResult::Failed;
	}

	// TODO: this options should be pass from the user land
	auto options = glslang_spv_options_t{ .generate_debug_info = true,
										  .strip_debug_info = false,
										  .disable_optimizer = true,
										  .optimize_size = false,
										  .disassemble = true,
										  .validate = true,
										  .emit_nonsemantic_shader_debug_info = false,
										  .emit_nonsemantic_shader_debug_source = false };

	// BUG: createModule function results a validation error
	[[maybe_unused]] auto optionsOptimized = glslang_spv_options_t{ .generate_debug_info = false,
																	.strip_debug_info = false,
																	.disable_optimizer = false,
																	.optimize_size = false,
																	.disassemble = true,
																	.validate = true,
																	.emit_nonsemantic_shader_debug_info = true,
																	.emit_nonsemantic_shader_debug_source = true };
	glslang_program_SPIRV_generate_with_options(program, stage, &options);

	byteCode.resize(glslang_program_SPIRV_get_size(program));
	glslang_program_SPIRV_get(program, byteCode.data());

	if (const char* spirvMessages = glslang_program_SPIRV_get_messages(program))
	{
		std::println("glslang messages: {}", spirvMessages);
	}
	glslang_program_delete(program);
	glslang_shader_delete(shader);

	glslang_finalize_process();
	return Utils::CompilationResult::Success;
}

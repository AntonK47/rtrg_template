#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <filesystem>
namespace Framework
{
	struct SceneInformation
	{
		uint32_t meshCount{};
	};

	struct VerticesStreamDeclaration
	{
		bool hasPosition{ false };
		bool hasNormal{ false };
		bool hasTangentBitangent{ false };
		bool hasTextureCoordinate0{ false };
		bool hasTextureCoordinate1{ false };
		bool hasColor{ false };
		// TODO: add animation related declarations
		// Optional: Stream compression
	};

	struct MeshImportSettings
	{
		bool applyOptimization{ false };
		std::vector<VerticesStreamDeclaration> verticesStreamDeclarations{};
		// TODO: add simplification options for LOD mesh generation
	};

	enum class AttributeSemantic
	{
		position,
		normal,
		tangentAndBitangent,
		textureCoordinate0,
		textureCoordinate1
	};

	struct AttributeDescriptor
	{
		AttributeSemantic semantic{AttributeSemantic::position};
		uint32_t offset{0};
		uint32_t stride{12};
		uint8_t componentSize{12};
		uint8_t componentCount{3};
	};

	struct VerticesStreamDescriptor
	{
		std::vector<AttributeDescriptor> attributes;
	};

	using StreamDataBuffer = std::vector<std::byte>;

	struct VertexStream
	{
		VerticesStreamDescriptor streamDescriptor;
		StreamDataBuffer data;
	};
	struct MeshData
	{
		std::vector<VertexStream> streams;
		StreamDataBuffer indexStream;
	};

	struct AssetImporter final
	{
		AssetImporter(const std::filesystem::path& filePath);
		~AssetImporter();

		AssetImporter(const AssetImporter&) = delete;
		AssetImporter(AssetImporter&&) = delete;

		MeshData ImportMesh(uint32_t meshIndex, const MeshImportSettings& meshImportSettings);

		const SceneInformation& GetSceneInformation() const
		{
			return sceneInformation;
		}

		bool HasLoadedScene() const
		{
			return currentlyLoadedScene != nullptr;
		}

	private:
		const aiScene* currentlyLoadedScene{nullptr};
		SceneInformation sceneInformation{};
		Assimp::Importer importer;
	};
} // namespace Framework
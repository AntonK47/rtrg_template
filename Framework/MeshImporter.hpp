#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

#include "Animation.hpp"


namespace Framework
{

	enum class MeshType
	{
		fixed,
		dynamic,
		skinned
	};

	struct MeshInfo
	{
		bool isSkinned{ false };
	};

	struct SceneInformation
	{
		U32 meshCount{};
		U32 texturesCount{};
		U32 animationCount{};
		U32 skeletonCount{};
		U32 materialCount{};
	};

	struct VerticesStreamDeclaration
	{
		bool hasPosition{ false };
		bool hasNormal{ false };
		bool hasTangentBitangent{ false };
		bool hasTextureCoordinate0{ false };
		bool hasTextureCoordinate1{ false };
		bool hasColor{ false };
		bool hasJointsIndexAndWeights{ false };
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
		textureCoordinate1,
		jointIndex,
		jointWeight
	};

	struct AttributeDescriptor
	{
		AttributeSemantic semantic{ AttributeSemantic::position };
		U32 offset{ 0 };
		U32 stride{ 12 };
		U8 componentSize{ 12 };
		U8 componentCount{ 3 };
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

		MeshData ImportMesh(U32 meshIndex, const MeshImportSettings& meshImportSettings);
		Animation::Skeleton ImportSkeleton(U32 meshIndex);
		Animation::AnimationDataSet LoadAllAnimations(const Animation::Skeleton& skeleton, const int resampleRate);

		const SceneInformation& GetSceneInformation() const
		{
			return sceneInformation;
		}

		bool HasLoadedScene() const
		{
			return currentlyLoadedScene != nullptr;
		}

		const Math::Matrix4x4 getModelMatrix(U32 meshIndex) const;

	private:
		const aiScene* currentlyLoadedScene{ nullptr };
		SceneInformation sceneInformation{};
		Assimp::Importer importer;
		std::unordered_map<std::string, Math::Matrix4x4> modelNameTransformMap{};
	};
} // namespace Framework
#include "MeshImporter.hpp"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <cassert>

Framework::AssetImporter::AssetImporter(const std::filesystem::path& filePath)
{

	auto scene = importer.ReadFile(filePath.generic_string(), 0);

	if (scene == nullptr)
	{
		return;
	}
	currentlyLoadedScene = (aiScene*)scene;

	sceneInformation.meshCount = scene->mNumMeshes;
}

Framework::AssetImporter::~AssetImporter()
{
	importer.FreeScene();
}

Framework::MeshData Framework::AssetImporter::ImportMesh(uint32_t meshIndex,
														 const MeshImportSettings& meshImportSettings)
{
	assert(meshIndex < currentlyLoadedScene->mNumMeshes);
	assert(currentlyLoadedScene->mMeshes[meshIndex]->HasPositions());
	bool shouldLoadNormalData = false;
	bool shouldLoadTangentData = false;
	bool shouldLoadTextureCoordinate0 = false;
	bool shouldLoadTextureCoordinate1 = false;

	for (const auto& streamDeclaration : meshImportSettings.verticesStreamDeclarations)
	{
		shouldLoadNormalData |= streamDeclaration.hasNormal;
		shouldLoadTangentData |= streamDeclaration.hasTangentBitangent;
		shouldLoadTextureCoordinate0 |= streamDeclaration.hasTextureCoordinate0;
		shouldLoadTextureCoordinate1 |= streamDeclaration.hasTextureCoordinate1;
	}

	unsigned int flags = aiProcess_Triangulate;
	if (!currentlyLoadedScene->mMeshes[meshIndex]->HasNormals() and shouldLoadNormalData)
	{
		flags |= aiProcess_GenNormals;
	}
	if (!currentlyLoadedScene->mMeshes[meshIndex]->HasTangentsAndBitangents() and shouldLoadTangentData)
	{
		flags |= aiProcess_CalcTangentSpace;
	}
	if (!currentlyLoadedScene->mMeshes[meshIndex]->HasTextureCoords(0) and shouldLoadTextureCoordinate0)
	{
		flags |= aiProcess_GenUVCoords;
	}
	if (!currentlyLoadedScene->mMeshes[meshIndex]->HasTextureCoords(1) and shouldLoadTextureCoordinate1)
	{
		flags |= aiProcess_GenUVCoords;
	}

	currentlyLoadedScene = importer.ApplyPostProcessing(flags);

	const auto& mesh = *currentlyLoadedScene->mMeshes[meshIndex];

	auto meshData = MeshData{};
	meshData.streams.reserve(meshImportSettings.verticesStreamDeclarations.size());

	for (const auto& streamDeclaration : meshImportSettings.verticesStreamDeclarations)
	{
		auto streamDescriptor = VerticesStreamDescriptor{};
		auto totalVertexSize = uint32_t{ 0 };
		auto positionOffset = uint32_t{ 0 };
		auto normalOffset = uint32_t{ 0 };
		auto tangentBitangentOffset = uint32_t{ 0 };
		auto textureCoordinate0Offset = uint32_t{ 0 };
		auto textureCoordinate1Offset = uint32_t{ 0 };
		if (streamDeclaration.hasPosition)
		{
			positionOffset = totalVertexSize;
			totalVertexSize += sizeof(aiVector3D);
			streamDescriptor.attributes.push_back(AttributeDescriptor{ .semantic = AttributeSemantic::position,
																	   .offset = positionOffset,
																	   .componentSize = sizeof(ai_real),
																	   .componentCount = 3 });
		}
		if (streamDeclaration.hasNormal)
		{
			normalOffset = totalVertexSize;
			totalVertexSize += sizeof(aiVector3D);
			streamDescriptor.attributes.push_back(AttributeDescriptor{ .semantic = AttributeSemantic::normal,
																	   .offset = normalOffset,
																	   .componentSize = sizeof(ai_real),
																	   .componentCount = 3 });
		}
		if (streamDeclaration.hasTangentBitangent)
		{
			tangentBitangentOffset = totalVertexSize;
			totalVertexSize += sizeof(aiVector3D) * 2;
			streamDescriptor.attributes.push_back(
				AttributeDescriptor{ .semantic = AttributeSemantic::tangentAndBitangent,
									 .offset = tangentBitangentOffset,
									 .componentSize = sizeof(ai_real),
									 .componentCount = 6 });
		}
		if (streamDeclaration.hasTextureCoordinate0)
		{
			textureCoordinate0Offset = totalVertexSize;
			totalVertexSize += sizeof(aiVector2D);
			streamDescriptor.attributes.push_back(
				AttributeDescriptor{ .semantic = AttributeSemantic::textureCoordinate0,
									 .offset = textureCoordinate0Offset,
									 .componentSize = sizeof(ai_real),
									 .componentCount = 2 });
		}
		if (streamDeclaration.hasTextureCoordinate1)
		{
			textureCoordinate1Offset = totalVertexSize;
			totalVertexSize += sizeof(aiVector2D);
			streamDescriptor.attributes.push_back(
				AttributeDescriptor{ .semantic = AttributeSemantic::textureCoordinate1,
									 .offset = textureCoordinate1Offset,
									 .componentSize = sizeof(ai_real),
									 .componentCount = 2 });
		}

		for (auto i = 0; i < streamDescriptor.attributes.size(); i++)
		{
			streamDescriptor.attributes[i].stride = totalVertexSize;
		}

		const auto totalStreamBufferSize = totalVertexSize * mesh.mNumVertices;
		auto data = StreamDataBuffer{};

		data.resize(totalStreamBufferSize);
		if (streamDeclaration.hasPosition)
		{
			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				std::memcpy(&data[i * totalVertexSize + positionOffset], &mesh.mVertices[i], sizeof(aiVector3D));
			}
		}
		if (streamDeclaration.hasNormal)
		{
			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				std::memcpy(&data[i * totalVertexSize + normalOffset], &mesh.mNormals[i], sizeof(aiVector3D));
			}
		}
		if (streamDeclaration.hasTangentBitangent)
		{
			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				std::memcpy(&data[i * totalVertexSize + tangentBitangentOffset], &mesh.mTangents[i],
							sizeof(aiVector3D));
				std::memcpy(&data[i * totalVertexSize + tangentBitangentOffset + sizeof(aiVector3D)],
							&mesh.mBitangents[i], sizeof(aiVector3D));
			}
		}
		if (streamDeclaration.hasTextureCoordinate0)
		{
			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				std::memcpy(&data[i * totalVertexSize + textureCoordinate0Offset], &mesh.mTextureCoords[0][i],
							sizeof(aiVector2D));
			}
		}
		if (streamDeclaration.hasTextureCoordinate1)
		{
			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				std::memcpy(&data[i * totalVertexSize + textureCoordinate1Offset], &mesh.mTextureCoords[1][i],
							sizeof(aiVector2D));
			}
		}

		meshData.streams.push_back(VertexStream{ .streamDescriptor = streamDescriptor, .data = data });
	}

	if (!meshImportSettings.verticesStreamDeclarations.empty())
	{

		const auto indexBufferSize = mesh.mNumFaces * 3 * sizeof(int);
		meshData.indexStream.resize(indexBufferSize);
		for (auto i = 0; i < mesh.mNumFaces; i++)
		{
			std::memcpy(meshData.indexStream.data() + 3 * sizeof(int) * i, mesh.mFaces[i].mIndices, 3 * sizeof(int));
		}
	}
	return meshData;
}

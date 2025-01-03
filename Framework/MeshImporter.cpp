#include "MeshImporter.hpp"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <queue>
#include <set>
#include <unordered_map>

using namespace Framework;
using namespace Framework::Animation;

namespace
{
	Math::Matrix4x4 ToMatrix4x4(aiMatrix4x4 matrix)
	{
		return Math::Matrix4x4{ glm::transpose(
			glm::mat4{ matrix.a1, matrix.a2, matrix.a3, matrix.a4, matrix.b1, matrix.b2, matrix.b3, matrix.b4,
					   matrix.c1, matrix.c2, matrix.c3, matrix.c4, matrix.d1, matrix.d2, matrix.d3, matrix.d4 }) };
	}
} // namespace

AssetImporter::AssetImporter(const std::filesystem::path& filePath)
{

	auto scene = importer.ReadFile(filePath.generic_string(), 0);

	if (scene == nullptr)
	{
		return;
	}
	currentlyLoadedScene = (aiScene*)scene;

	sceneInformation.meshCount = scene->mNumMeshes;
	sceneInformation.animationCount = scene->mNumAnimations;
	sceneInformation.materialCount = scene->mNumMaterials;
	sceneInformation.texturesCount = scene->mNumTextures;
	sceneInformation.skeletonCount = scene->mNumSkeletons;


	std::queue<aiNode*> nodes;

	nodes.push(currentlyLoadedScene->mRootNode);

	while (not nodes.empty())
	{
		auto node = nodes.front();
		nodes.pop();

		const auto key = std::string{ node->mName.C_Str() };
		auto value = ToMatrix4x4(node->mTransformation);

		if (node->mParent)
		{
			const auto parentKey = std::string{ node->mParent->mName.C_Str() };

			value = modelNameTransformMap[parentKey] * value;
		}

		modelNameTransformMap[key] = value;

		for (auto i = 0; i < node->mNumChildren; i++)
		{
			nodes.push(node->mChildren[i]);
		}
	}
}

AssetImporter::~AssetImporter()
{
	importer.FreeScene();
}

MeshData AssetImporter::ImportMesh(U32 meshIndex, const MeshImportSettings& meshImportSettings)
{
	assert(meshIndex < currentlyLoadedScene->mNumMeshes);
	assert(currentlyLoadedScene->mMeshes[meshIndex]->HasPositions());
	bool shouldLoadNormalData = false;
	bool shouldLoadTangentData = false;
	bool shouldLoadTextureCoordinate0 = false;
	bool shouldLoadTextureCoordinate1 = false;
	bool shouldLoadJointsIndexAndWeight = false;

	for (const auto& streamDeclaration : meshImportSettings.verticesStreamDeclarations)
	{
		shouldLoadNormalData |= streamDeclaration.hasNormal;
		shouldLoadTangentData |= streamDeclaration.hasTangentBitangent;
		shouldLoadTextureCoordinate0 |= streamDeclaration.hasTextureCoordinate0;
		shouldLoadTextureCoordinate1 |= streamDeclaration.hasTextureCoordinate1;
		shouldLoadJointsIndexAndWeight |= streamDeclaration.hasJointsIndexAndWeights;
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

	if (shouldLoadJointsIndexAndWeight)
	{
		// assert(mesh.HasBones()); // TODO:
	}

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
		auto jointsIndexOffset = uint32_t{ 0 };
		auto jointsWeightOffset = uint32_t{ 0 };
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

		if (streamDeclaration.hasJointsIndexAndWeights)
		{
			jointsIndexOffset = totalVertexSize;
			totalVertexSize += sizeof(uint32_t);
			streamDescriptor.attributes.push_back(AttributeDescriptor{ .semantic = AttributeSemantic::jointIndex,
																	   .offset = jointsIndexOffset,
																	   .componentSize = sizeof(uint32_t),
																	   .componentCount = 1 });
			jointsWeightOffset = totalVertexSize;
			totalVertexSize += sizeof(float) * 4;
			streamDescriptor.attributes.push_back(AttributeDescriptor{ .semantic = AttributeSemantic::jointWeight,
																	   .offset = jointsWeightOffset,
																	   .componentSize = sizeof(float),
																	   .componentCount = 4 });
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
			const auto t = mesh.HasTextureCoords(0);
			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				if (t)
				{

					std::memcpy(&data[i * totalVertexSize + textureCoordinate0Offset], &mesh.mTextureCoords[0][i],
								sizeof(aiVector2D));
				}
				else
				{
					auto m = aiVector2D{ 0.0f, 0.0f };
					std::memcpy(&data[i * totalVertexSize + textureCoordinate0Offset], &m, sizeof(aiVector2D));
				}
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
		if (streamDeclaration.hasJointsIndexAndWeights)
		{
			const auto skeleton = ImportSkeleton(meshIndex);
			struct JointVertexData
			{
				int jointIndex;
				float weight;
			};
			std::vector<std::vector<JointVertexData>> v;
			v.resize(mesh.mNumVertices);

			for (auto i = 0; i < mesh.mNumBones; i++)
			{
				auto& bone = *mesh.mBones[i];

				auto it = std::find_if(skeleton.joints.begin(), skeleton.joints.end(), [&](const Joint& joint)
									   { return joint.name == std::string{ bone.mName.C_Str() }; });
				if (it == skeleton.joints.end())
				{
					continue;
				}

				const auto jointIndex = (int)std::distance(skeleton.joints.begin(), it);

				for (auto j = 0; j < bone.mNumWeights; j++)
				{
					if (bone.mWeights[j].mWeight > 0.01)
					{
						v[bone.mWeights[j].mVertexId].push_back(
							JointVertexData{ jointIndex, bone.mWeights[j].mWeight });
					}
				}
			}

			for (auto i = 0; i < v.size(); i++)
			{
				std::sort(v[i].begin(), v[i].end(),
						  [](const JointVertexData& s1, const JointVertexData& s2) { return s1.weight > s2.weight; });
			}
			for (auto i = 0; i < v.size(); i++)
			{
				v[i].resize(4);
				auto totalWeight = 0.0f;
				for (auto j = 0; j < v[i].size(); j++)
				{
					totalWeight += v[i][j].weight;
				}
				for (auto j = 0; j < v[i].size(); j++)
				{
					v[i][j].weight /= totalWeight;
				}
			}

			for (auto i = 0; i < mesh.mNumVertices; i++)
			{
				const auto jointIndicies = glm::vec<4, uint8_t>{ v[i][0].jointIndex, v[i][1].jointIndex,
																 v[i][2].jointIndex, v[i][3].jointIndex };
				const auto jointWeights = glm::vec4{ v[i][0].weight, v[i][1].weight, v[i][2].weight, v[i][3].weight };

				std::memcpy(&data[i * totalVertexSize + jointsIndexOffset], &jointIndicies,
							sizeof(glm::vec<4, uint8_t>));
				std::memcpy(&data[i * totalVertexSize + jointsWeightOffset], &jointWeights, sizeof(glm::vec4));
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

Skeleton AssetImporter::ImportSkeleton(U32 meshIndex)
{
	assert(meshIndex < currentlyLoadedScene->mNumMeshes);

	currentlyLoadedScene = importer.ApplyPostProcessing(aiProcess_PopulateArmatureData);
	const auto& mesh = currentlyLoadedScene->mMeshes[meshIndex];

	assert(mesh->HasBones());

	auto animationRoot = mesh->mBones[0]->mArmature;

	while (animationRoot->mParent != nullptr)
	{
		animationRoot = animationRoot->mParent;
	}
	auto skeleton = Skeleton{};
	{
		std::queue<aiNode*> children;
		children.push(animationRoot);
		auto totalJoints = 0;
		while (!children.empty())
		{
			auto node = children.front();
			children.pop();

			for (auto i = 0; i < node->mNumChildren; i++)
			{
				children.push(node->mChildren[i]);
			}
			totalJoints++;
		}
	}

	std::unordered_map<std::string, glm::mat4> boneMatrix;
	for (auto i = 0; i < mesh->mNumBones; i++)
	{
		const auto matrix = glm::transpose(glm::mat4{
			mesh->mBones[i]->mOffsetMatrix.a1, mesh->mBones[i]->mOffsetMatrix.a2, mesh->mBones[i]->mOffsetMatrix.a3,
			mesh->mBones[i]->mOffsetMatrix.a4, mesh->mBones[i]->mOffsetMatrix.b1, mesh->mBones[i]->mOffsetMatrix.b2,
			mesh->mBones[i]->mOffsetMatrix.b3, mesh->mBones[i]->mOffsetMatrix.b4, mesh->mBones[i]->mOffsetMatrix.c1,
			mesh->mBones[i]->mOffsetMatrix.c2, mesh->mBones[i]->mOffsetMatrix.c3, mesh->mBones[i]->mOffsetMatrix.c4,
			mesh->mBones[i]->mOffsetMatrix.d1, mesh->mBones[i]->mOffsetMatrix.d2, mesh->mBones[i]->mOffsetMatrix.d3,
			mesh->mBones[i]->mOffsetMatrix.d4 });
		boneMatrix[std::string{ mesh->mBones[i]->mName.C_Str() }] = matrix;
	}


	std::unordered_set<std::string> animatedBondes;
	for (auto i = 0; i < mesh->mNumBones; i++)
	{
		auto bone = mesh->mBones[i]->mNode;
		animatedBondes.insert(std::string{ bone->mName.C_Str() });

		while (bone->mParent->mParent != nullptr)
		{
			bone = bone->mParent;
			animatedBondes.insert(std::string{ bone->mName.C_Str() });
		}
	}

	struct Node
	{
		aiNode* node;
		int32_t parentIndex;
	};
	std::queue<Node> children;
	children.push({ animationRoot, -1 });

	while (!children.empty())
	{
		auto [node, parentIndex] = children.front();
		children.pop();
		auto index = (int32_t)skeleton.joints.size();
		if (animatedBondes.contains(std::string{ node->mName.C_Str() }))
		{
			const auto inverseTransform = glm::inverse(glm::transpose(glm::mat4{
				node->mTransformation.a1, node->mTransformation.a2, node->mTransformation.a3, node->mTransformation.a4,
				node->mTransformation.b1, node->mTransformation.b2, node->mTransformation.b3, node->mTransformation.b4,
				node->mTransformation.c1, node->mTransformation.c2, node->mTransformation.c3, node->mTransformation.c4,
				node->mTransformation.d1, node->mTransformation.d2, node->mTransformation.d3,
				node->mTransformation.d4 }));


			skeleton.joints.push_back(Joint{ boneMatrix[std::string{ node->mName.C_Str() }], inverseTransform,
											 parentIndex, node->mName.C_Str() });
		}


		for (auto i = 0; i < node->mNumChildren; i++)
		{

			children.push({ node->mChildren[i], index });
		}
	}

	return skeleton;
}

AnimationDataSet AssetImporter::LoadAllAnimations(const Skeleton& skeleton, const int resampleRate)
{
	std::vector<JointAnimationData> animationDatabase;
	std::vector<AnimationData> animationDataResult;
	std::unordered_map<std::string, int> skeletonJointNameToOffsetMap;
	for (auto i = 0; i < skeleton.joints.size(); i++)
	{
		skeletonJointNameToOffsetMap.insert(std::make_pair(skeleton.joints[i].name, i));
	}

	for (auto i = 0; i < currentlyLoadedScene->mNumAnimations; i++)
	{
		auto& animation = *currentlyLoadedScene->mAnimations[i];


		const auto durationInSeconds = animation.mDuration / animation.mTicksPerSecond;

		const auto framesPerAnimation = (int)((resampleRate * durationInSeconds * 1001.0f) / 1000.0f);
		const auto skeletonJointsCount = skeleton.joints.size();

		std::vector<JointAnimationData> animationData{};
		animationData.resize(framesPerAnimation * skeletonJointsCount);
		struct PositionSample
		{
			float time;
			glm::vec3 position;
		};

		struct RotationSample
		{
			float time;
			glm::quat rotation;
		};
		auto resamplePostion = [](const std::vector<PositionSample>& positionSamples,
								  int targetFps) -> std::vector<glm::vec3>
		{
			assert(positionSamples.size() >= 1);

			const auto start = 0.0f; // positionSamples.front().time;
			const auto end = positionSamples.back().time;

			const auto duration = end - start;
			const auto timePerFrame = 1.0f / (float)targetFps;

			const auto frames = std::max(1, (int)(((float)targetFps * duration * 1001.0f) / 1000.0f));


			std::vector<glm::vec3> result;
			result.resize(frames);

			auto t = start;

			result[0] = positionSamples.front().position;
			result[frames - 1] = positionSamples.back().position;
			int a = 0;
			int b = 1;

			for (int i = 1; i < frames - 1; i++)
			{
				t += timePerFrame;
				while (positionSamples[a].time + timePerFrame < t)
				{
					a++;
				}
				a--;
				a = std::max(a, 0);
				b = a + 1;
				// assert(positionSamples[a].time < t && positionSamples[b].time >= t);
				{
					const float sa = positionSamples[a].time;
					const float sb = positionSamples[b].time;
					const float rest = t - sa;
					const float factor = rest / (sb - sa);

					const auto attribute1 = positionSamples[a].position;
					const auto attribute2 = positionSamples[b].position;


					result[i] = attribute1 * (1.0f - factor) + factor * attribute2;
				}
			}
			return result;
		};

		auto resampleRotation = [](const std::vector<RotationSample>& rotationSamples,
								   int targetFps) -> std::vector<glm::quat>
		{
			assert(rotationSamples.size() >= 1);

			const auto start = 0.0f; // rotationSamples.front().time;
			const auto end = rotationSamples.back().time;

			const auto duration = end - start;
			const auto timePerFrame = 1.0f / (float)targetFps;

			const auto frames = std::max(1, (int)((targetFps * duration * 1001.0f) / 1000.0f));


			std::vector<glm::quat> result;
			result.resize(frames);

			auto t = start;

			result[0] = rotationSamples.front().rotation;
			result[frames - 1] = rotationSamples.back().rotation;
			int a = 0;
			int b = 1;

			for (int i = 1; i < frames - 1; i++)
			{
				t += timePerFrame;
				while (rotationSamples[a].time + timePerFrame < t)
				{
					a++;
				}
				a--;
				a = std::max(a, 0);
				b = a + 1;
				// assert(rotationSamples[a].time < t && rotationSamples[b].time >= t);
				{
					const float sa = rotationSamples[a].time;
					const float sb = rotationSamples[b].time;
					const float rest = t - sa;
					const float factor = rest / (sb - sa);

					const auto attribute1 = rotationSamples[a].rotation;
					const auto attribute2 = rotationSamples[b].rotation;


					result[i] = glm::slerp(attribute1, attribute2, factor);
				}
			}
			return result;
		};
		const auto seconds = animation.mDuration / animation.mTicksPerSecond;
		for (auto channelIndex = 0; channelIndex < animation.mNumChannels; channelIndex++)
		{
			auto& channel = *animation.mChannels[channelIndex];
			const auto channelNodeName = std::string{ channel.mNodeName.C_Str() };

			if (skeletonJointNameToOffsetMap.contains(channelNodeName))
			{
				const auto offset = skeletonJointNameToOffsetMap[channelNodeName];

				{
					auto positionSamples = std::vector<PositionSample>{};
					positionSamples.resize(channel.mNumPositionKeys);

					for (auto t = 0; t < channel.mNumPositionKeys; t++)
					{
						positionSamples[t] = { (float)(channel.mPositionKeys[t].mTime / animation.mTicksPerSecond),
											   glm::vec3{ channel.mPositionKeys[t].mValue.x,
														  channel.mPositionKeys[t].mValue.y,
														  channel.mPositionKeys[t].mValue.z } };
					}

					std::vector<glm::vec3> resampledPositions = resamplePostion(positionSamples, resampleRate);

					for (auto i = 0; i < resampledPositions.size(); i++)
					{
						animationData[i * skeletonJointsCount + offset].translation = resampledPositions[i];
					}
				}
				{
					auto rotationSamples = std::vector<RotationSample>{};
					rotationSamples.resize(channel.mNumRotationKeys);

					for (auto t = 0; t < channel.mNumRotationKeys; t++)
					{
						rotationSamples[t] = { (float)(channel.mRotationKeys[t].mTime / animation.mTicksPerSecond),
											   glm::quat{
												   channel.mRotationKeys[t].mValue.w,
												   channel.mRotationKeys[t].mValue.x,
												   channel.mRotationKeys[t].mValue.y,
												   channel.mRotationKeys[t].mValue.z,
											   } };
					}
					std::vector<glm::quat> resampledRotations = resampleRotation(rotationSamples, resampleRate);

					for (auto i = 0; i < resampledRotations.size(); i++)
					{
						animationData[i * skeletonJointsCount + offset].rotation = resampledRotations[i];
					}
				}
			}
		}

		animationDataResult.push_back(
			AnimationData{ .offset = static_cast<uint32_t>(animationDatabase.size()),
						   .count = static_cast<uint32_t>(skeleton.joints.size()),
						   .frames = static_cast<uint32_t>(framesPerAnimation),
						   .duration = (float)(animation.mDuration / animation.mTicksPerSecond),
						   .animationName = animation.mName.C_Str() });
		animationDatabase.insert_range(animationDatabase.end(), animationData);
	}

	return AnimationDataSet{ animationDataResult, animationDatabase };
}

const Math::Matrix4x4 Framework::AssetImporter::getModelMatrix(U32 meshIndex) const
{
	return modelNameTransformMap.at(std::string{ currentlyLoadedScene->mMeshes[meshIndex]->mName.C_Str() });
}

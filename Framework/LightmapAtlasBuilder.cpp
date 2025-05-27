#include "LightmapAtlasBuilder.hpp"

#include <SDL3/SDL_surface.h>
#include <assimp/Importer.hpp>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

using namespace Framework;
using namespace Framework::Math;

LightmapAtlasBuilder::LightmapAtlasBuilder()
{
	atlas = xatlas::Create();
}

void Framework::LightmapAtlasBuilder::AddMesh(const MeshData& mesh, const Math::Vector3& positionScale)
{
	const auto streamDescriptor = mesh.streams.front().streamDescriptor;

	auto vertexSize = 0u;
	auto positionStride = 0u;
	for (auto j = 0; j < mesh.streams[0].streamDescriptor.attributes.size(); j++)
	{
		vertexSize += mesh.streams[0].streamDescriptor.attributes[j].componentCount *
			mesh.streams[0].streamDescriptor.attributes[j].componentSize;
		positionStride += mesh.streams[0].streamDescriptor.attributes[j].stride;
	}
	const auto vertexCount = static_cast<U32>(mesh.streams[0].data.size() / vertexSize);
	const auto indexCount = static_cast<U32>(mesh.indexStream.size() / 4);

	for (auto i = 0; i < vertexCount; i++)
	{
		auto positionPointer = (Math::Vector3*)(mesh.streams[0].data.data());
		positionPointer += i;
		*positionPointer = (*positionPointer) * positionScale;
	}

	auto meshDecl = xatlas::MeshDecl{ .vertexPositionData = mesh.streams.front().data.data(),
									  .indexData = mesh.indexStream.data(),
									  .vertexCount = vertexCount,
									  .vertexPositionStride = positionStride,
									  .indexCount = indexCount,
									  .indexFormat = xatlas::IndexFormat::UInt32 };

	auto result = xatlas::AddMesh(atlas, meshDecl);
	assert(result == xatlas::AddMeshError::Success);
}

void LightmapAtlasBuilder::Build()
{
	xatlas::ComputeCharts(atlas, { .maxIterations = 20 });
	xatlas::PackCharts(
		atlas,
		xatlas::PackOptions{
			.resolution = U32{ 2u * 1024u }, .blockAlign = true, .createImage = true, .rotateCharts = false });

	struct ChartTransform
	{
		Vector2 scale;
		Vector2 offset;
	};

	struct InstanceCharts
	{
		U32 chartOffset;
		U32 chartCount;
	};

	std::vector<ChartTransform> chartTransforms;
	std::vector<InstanceCharts> instanceCharts;

	chartTransforms.resize(atlas->chartCount);
	instanceCharts.resize(atlas->meshCount);

	auto chartOffset = U32{ 0 };

	for (auto i = 0; i < atlas->meshCount; i++)
	{
		const auto& mesh = atlas->meshes[i];
		for (auto j = 0; j < mesh.chartCount; j++)
		{
			const auto& chart = mesh.chartArray[j];
			auto box = Box2{};
			for (auto k = 0; k < chart.faceCount; k++)
			{
				const auto index0 = mesh.indexArray[k * 3 + 0];
				const auto index1 = mesh.indexArray[k * 3 + 1];
				const auto index2 = mesh.indexArray[k * 3 + 2];

				const auto vertex0 = mesh.vertexArray[index0];
				const auto vertex1 = mesh.vertexArray[index1];
				const auto vertex2 = mesh.vertexArray[index2];

				box.Extend(Vector2{ vertex0.uv[0], vertex0.uv[1] });
				box.Extend(Vector2{ vertex1.uv[0], vertex1.uv[1] });
				box.Extend(Vector2{ vertex2.uv[0], vertex2.uv[1] });
			}

			const auto scale = box.Extent();
			const auto offset = box.lower;

			chartTransforms[chartOffset + j] = ChartTransform{ scale, offset };
		}

		instanceCharts[i] = InstanceCharts{ chartOffset, mesh.chartCount };
		chartOffset += mesh.chartCount;
	}

	auto surface = SDL_CreateSurface(atlas->width, atlas->height, SDL_PIXELFORMAT_ABGR32);

	std::vector<uint8_t> imageData;
	imageData.resize(atlas->width * atlas->height * 4);
	for (auto i = 0; i < atlas->width * atlas->height; i++)
	{
		imageData[i * 4 + 0] = uint8_t(255);
		imageData[i * 4 + 1] = uint8_t((atlas->image[i] >> 0) % 255);
		imageData[i * 4 + 2] = uint8_t((atlas->image[i] >> 8) % 255);
		imageData[i * 4 + 3] = uint8_t((atlas->image[i] >> 16) % 255);
	}

	surface->pixels = imageData.data();
	SDL_SaveBMP(surface, "atlas.bmp");
}

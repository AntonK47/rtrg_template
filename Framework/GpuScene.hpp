#pragma once

#include "Core.hpp"
#include "Math.hpp"

namespace Framework
{

	struct GpuScene
	{
		struct Aabb
		{
			Math::Vector3 min;
			Math::Vector3 max;
		};

		struct GpuGeometry
		{
			U32 indexByteBase;
			U32 vertexByteBase;
			U32 indexCount;
			U32 vertexCount;
			U32 vertexStride;
		};

		struct GpuSubMesh
		{
			GpuGeometry geometry;
		};

		struct GpuLodMesh
		{
			U32 gpuSubMeshBase;
			U32 gpuSubMeshCount;
			float distanceBegin;
			float distanceEnd;
		};

		struct GpuMesh
		{
			U32 gpuLodMeshIndicies[5];
			U32 gpuLodMeshCount;
			Aabb boundingBox;
		};
	};

} // namespace Framework
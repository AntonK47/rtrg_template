#pragma once

#include "Core.hpp"
#include <MeshImporter.hpp>

#include <xatlas.h>


namespace Framework
{
	struct LightmapAtlasBuilder final
	{
		LightmapAtlasBuilder();

		void AddMesh(const MeshData& mesh, const Math::Vector3& positionScale);
		void Build();

		private:
			xatlas::Atlas* atlas;
	};
}
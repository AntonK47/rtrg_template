#ifndef GPU_SCENE_LIBRARY_GLSL
#define GPU_SCENE_LIBRARY_GLSL

#ifndef GPU_SCENE_LIBRARY_SET
	#define GPU_SCENE_LIBRARY_SET 0
#endif

#ifndef GPU_SCENE_LIBRARY_BINDING
	#define GPU_SCENE_LIBRARY_BINDING 4
#endif

#ifdef GPU_PERSISTEND_MEMORY_READONLY
	#define BUFFER_ACCESS readonly
#else
	#define BUFFER_ACCESS
#endif

layout(set = GPU_SCENE_LIBRARY_SET, binding = GPU_SCENE_LIBRARY_BINDING) BUFFER_ACCESS buffer gpuPersistentMemoryBlock
{
	uint gpuPersistentMemory[];
};

layout(set = GPU_SCENE_LIBRARY_SET, binding = GPU_SCENE_LIBRARY_BINDING+1) buffer uploadHeapMemoryBlock
{
	uint uploadHeapMemory[];
};

struct Aabb
{
	vec3 min;
	vec3 max;
};

struct GpuGeometry
{
	uint indexByteBase;
	uint vertexByteBase;
	uint indexCount;
	uint vertexCount;
	uint vertexStride;
};

struct GpuSubMesh
{
	GpuGeometry geometry;
};

struct GpuLodMesh
{
	uint gpuSubMeshBase;
	uint gpuSubMeshCount;
	float distanceBegin;
	float distanceEnd;
};

struct GpuMesh
{
	uint gpuLodMeshBase;
	uint gpuLodMeshCount;
	Aabb boundingBox;
};


struct GpuTableHeader
{
	uint byteOffset;
	uint size;
	uint reservedSize;
};

#define GPU_TABLE_HEADER_SIZE 3
#define AABB_SIZE 6
#define GPU_MESH_SIZE  (2 + AABB_SIZE)
#define GPU_LOD_MESH_SIZE 10
#define GPU_SUB_MESH_SIZE 5

struct GpuMeshTable
{
	GpuTableHeader header;
};


GpuTableHeader gpu_table_header_decode(in uint byteOffset)
{
	GpuTableHeader header;
	header.byteOffset = byteOffset;
	header.size = gpuPersistentMemory[byteOffset];
	header.reservedSize = gpuPersistentMemory[byteOffset + 1];
	return header;
}

void gpu_table_header_encode(in GpuTableHeader header)
{
	gpuPersistentMemory[header.byteOffset] = header.size;
	gpuPersistentMemory[header.byteOffset + 1] = header.reservedSize;
}

GpuMeshTable gpu_mesh_table_create(in uint byteOffset)
{
	GpuMeshTable table;
	table.header = gpu_table_header_decode(byteOffset);
	return table;
}

Aabb aabb_decode(in uint byteOffset)
{
	Aabb aabb;

	uint packedValue0 = gpuPersistentMemory[byteOffset];
	uint packedValue1 = gpuPersistentMemory[byteOffset+1];
	uint packedValue2 = gpuPersistentMemory[byteOffset+2];
	uint packedValue3 = gpuPersistentMemory[byteOffset];
	uint packedValue4 = gpuPersistentMemory[byteOffset+1];
	uint packedValue5 = gpuPersistentMemory[byteOffset+2];
	float value0 = uintBitsToFloat(packedValue0);
	float value1 = uintBitsToFloat(packedValue1);
	float value2 = uintBitsToFloat(packedValue2);
	float value3 = uintBitsToFloat(packedValue3);
	float value4 = uintBitsToFloat(packedValue4);
	float value5 = uintBitsToFloat(packedValue5);


	aabb.min =  vec3(value0, value1, value2);
	aabb.max =  vec3(value3, value4, value5);
	return aabb;
}

GpuMesh gpu_mesh_decode(in uint byteOffset)
{
	GpuMesh gpuMesh;
	gpuMesh.gpuLodMeshBase = gpuPersistentMemory[byteOffset];
	gpuMesh.gpuLodMeshCount = gpuPersistentMemory[byteOffset + 1];
	gpuMesh.boundingBox = aabb_decode(byteOffset + 2);
	return gpuMesh;
}

GpuMesh gpu_mesh_table_get_at(in GpuMeshTable table, in uint index)
{
	uint byteOffset = table.header.byteOffset + GPU_TABLE_HEADER_SIZE + index * GPU_MESH_SIZE;
	return gpu_mesh_decode(byteOffset);
}

GpuLodMesh gpu_lod_mesh_decode(in uint byteOffset)
{
	GpuLodMesh lodMesh;
	lodMesh.gpuSubMeshBase = gpuPersistentMemory[byteOffset];
	lodMesh.gpuSubMeshCount = gpuPersistentMemory[byteOffset + 1];
	lodMesh.distanceBegin = uintBitsToFloat(gpuPersistentMemory[byteOffset + 2]);
	lodMesh.distanceEnd = uintBitsToFloat(gpuPersistentMemory[byteOffset + 3]);
	return lodMesh;
}

struct GpuLodMeshTable
{
	GpuTableHeader header;
};

GpuLodMeshTable gpu_lod_mesh_table_create(in uint byteOffset)
{
	GpuLodMeshTable table;
	table.header = gpu_table_header_decode(byteOffset);
	return table;
}

GpuLodMesh gpu_lod_mesh_table_get_at(in GpuLodMeshTable table, in uint index)
{
	uint byteOffset = table.header.byteOffset + GPU_TABLE_HEADER_SIZE + index * GPU_LOD_MESH_SIZE;
	return gpu_lod_mesh_decode(byteOffset);
}

GpuSubMesh gpu_sub_mesh_decode(in uint byteOffset)
{
	GpuSubMesh subMesh;
	subMesh.geometry.indexByteBase = gpuPersistentMemory[byteOffset];
	subMesh.geometry.vertexByteBase = gpuPersistentMemory[byteOffset + 1];
	subMesh.geometry.indexCount = gpuPersistentMemory[byteOffset + 2];
	subMesh.geometry.vertexCount = gpuPersistentMemory[byteOffset + 3];
	subMesh.geometry.vertexStride = gpuPersistentMemory[byteOffset + 4];
	return subMesh;
}

struct GpuSubMeshTable
{
	GpuTableHeader header;
};

GpuSubMeshTable gpu_sub_mesh_table_create(in uint byteOffset)
{
	GpuSubMeshTable table;
	table.header = gpu_table_header_decode(byteOffset);
	return table;
}

GpuSubMesh gpu_sub_mesh_table_get_at(in GpuSubMeshTable table, in uint index)
{
	uint byteOffset = table.header.byteOffset + GPU_TABLE_HEADER_SIZE + index * GPU_SUB_MESH_SIZE;
	return gpu_sub_mesh_decode(byteOffset);
}

struct GpuMeshInstance
{
	uint gpuMeshIndex;
	mat4 modelMatrix;
};

struct GpuMeshInstanceTable
{
	GpuTableHeader header;
};

#define TRANSFORM_MATRIX_4x3_SIZE 12
#define GPU_MESH_INSTANCE_SIZE (TRANSFORM_MATRIX_4x3_SIZE + 1)

mat4 transform_matrix_decode(in uint byteOffset)
{
	mat4 matrix;
	
	matrix[0] = vec4(
	uintBitsToFloat(gpuPersistentMemory[byteOffset]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 1]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 2]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 3]));

	matrix[1] = vec4(
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 4]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 5]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 6]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 7]));

	matrix[2] = vec4(
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 8]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 9]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 10]),
	uintBitsToFloat(gpuPersistentMemory[byteOffset + 11]));

	matrix[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
	return matrix;
}

GpuMeshInstance gpu_mesh_instance_decode(in uint byteOffset)
{
	GpuMeshInstance instance;
	instance.gpuMeshIndex = gpuPersistentMemory[byteOffset];
	instance.modelMatrix = transform_matrix_decode(byteOffset + 1);
	return instance;
}

GpuMeshInstanceTable gpu_mesh_instance_table_create(in uint byteOffset)
{
	GpuMeshInstanceTable table;
	table.header = gpu_table_header_decode(byteOffset);
	return table;
}

GpuMeshInstance gpu_mesh_tnstance_table_get_at(in GpuMeshInstanceTable table, in uint index)
{
	uint byteOffset = table.header.byteOffset + GPU_TABLE_HEADER_SIZE + index * GPU_MESH_INSTANCE_SIZE;
	return gpu_mesh_instance_decode(byteOffset);
}

#endif
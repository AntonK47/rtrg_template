#pragma once

#include "Core.hpp"
#include "Pool.hpp"
#include "Scene.hpp"
#include "StreamingSystem.hpp"
#include "VulkanRHI.hpp"

#include <rigtorp/MPMCQueue.h>

#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>


namespace Framework
{
	struct WorkgroupItemArguments
	{
		U32 argumentsOffset;
		U32 count;
	};

	struct DataSource
	{

		const std::span<std::byte> GetData() const
		{
			return OnGetData();
		}

		virtual ~DataSource()
		{
		}

	protected:
		virtual const std::span<std::byte> OnGetData() const = 0;
	};

	struct FileDataSource : public DataSource
	{
		FileDataSource(const std::filesystem::path& file);

		const std::span<std::byte> OnGetData() const override;

	private:
		std::filesystem::path filePath;
	};

	struct UnifiedGeometryBuffer
	{
		struct VirtualSubMesh
		{
			U32 byteOffset;
			U32 byteSize;
		};

		struct Page
		{
			U32 physicalPageIndex;
		};

		struct PageInfo
		{
			bool isLoaded{ false };
			bool loadingIsPending{ false };
			bool isDirty{ false };
			bool isDeletionRequested{ false };
		};

		using PageIndex = U32;

		struct LookupTableEntry
		{
			U32 physicalPageIndex;
		};

		static constexpr U32 pendingPageCount = 1024;
		static constexpr U32 totalPages = 1024;
		static constexpr U32 pageSizeInBytes = 4 * 1024;

		struct PageUploadRequest
		{
			PageIndex pageIndex;
			VirtualSubMesh virtualSubMesh;
			std::unique_ptr<DataSource> dataSource;
		};

		rigtorp::MPMCQueue<PageIndex> finishedUploads{ pendingPageCount };

		std::array<PageInfo, totalPages> pages;

		Pool<LookupTableEntry, totalPages> lookupTable;

		/*void UploadPage(const PageInfo& page, const std::span<std::byte>& data);*/

		StreamingSystem* streamingSystem_{ nullptr };

		Scene* scene_;
		Graphics::VulkanContext* vulkanContext_;

		mutable std::mutex requestLock;

		void RequestSubMesh(const VirtualSubMesh& subMesh);

		void CreateResources(Graphics::VulkanContext& context, Scene& scene, StreamingSystem& streamingSystem);
		void ReleaseResources(const Graphics::VulkanContext& context);

		void UpdateUnifiedGeometryBufferLookupTable(VkCommandBuffer cmd, const Scene& scene);

		Graphics::GraphicsBuffer geometryLookupTableBuffer{};
		Graphics::ComputePipeline lookupTableUpdatePipeline{};
		Graphics::PipelineLayout lookupTableUpdatePipelineLayout{};
		Graphics::BindGroupLayout geometryLookupTableBindGroupLayout{};
		VkDescriptorSet geometryLookupTableDescriptorSet{};
		VkDescriptorPool geometryLookupTableDescriptorPool{};
	};
} // namespace Framework
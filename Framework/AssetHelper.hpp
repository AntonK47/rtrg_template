#pragma once

#include <Core.hpp>
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stduuid/uuid.h>
#include <string>

namespace nlohmann
{
	void to_json(nlohmann::json& json, const uuids::uuid& uuid)
	{
		json = nlohmann::json{ { "uuid", uuids::to_string(uuid) } };
	}


	void from_json(const nlohmann::json& json, uuids::uuid& uuid)
	{
		auto value = std::string{};
		json.at("uuid").get_to(value);
		uuid = uuid.from_string(value).value();
	}
} // namespace nlohmann

namespace Framework
{
	enum class AssetType : U8
	{
		subMesh
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(AssetType, { { AssetType::subMesh, "subMesh" } });

	struct AssetNode
	{
		uuids::uuid uuid;
		std::string name;
		AssetType type;
		U32 version;
		std::string assetNodeData;

	public:
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(AssetNode, uuid, name, type, version, assetNodeData);
	};

	struct AssetFile
	{
		uuids::uuid uuid;
		std::string name;
		U32 version;
		std::vector<AssetNode> assets;

	public:
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(AssetFile, uuid, name, version, assets);
	};

	enum class MeshType : U8
	{
		skinned
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(MeshType, { { MeshType::skinned, "skinned" } });

	struct BinarySourceFile
	{
		U32 offset;
		U32 size;
		std::string file;

	public:
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(BinarySourceFile, offset, size, file);
	};

	struct MeshAsset
	{
		uuids::uuid uuid;
		std::string name;
		MeshType type;
		BinarySourceFile data;

	public:
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshAsset, uuid, name, type, data);
	};

	struct SceneBuilder
	{
	};

	struct Asset
	{
		template <typename T>
		void Store(const std::string& assetFile)
		{
		}

		template <typename T>
		void Load(SceneBuilder& scene, const std::string& assetFile)
		{
		}

		template <>
		void Load<MeshAsset>(SceneBuilder& scene, const std::string& assetFile)
		{
			const auto assetFilePath = std::filesystem::path{ assetFile };
			assert(std::filesystem::exists(assetFilePath));
			std::ifstream f(assetFile);
			auto data = nlohmann::json::parse(f);
			MeshAsset asset;
			asset = data.get<MeshAsset>();

			// SceneBuilder
		}

		template <>
		void Store<MeshAsset>(const std::string& assetFile)
		{
		}
	};

} // namespace Framework
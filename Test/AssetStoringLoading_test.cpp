#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <AssetHelper.hpp>


using namespace Framework;

TEST(AssetStoringAndLoading, StoreAsset)
{
	auto unitTest = testing::UnitTest::GetInstance();

	const auto workingDirectory = std::filesystem::path(unitTest->original_working_dir());

	auto uuidGenerator = uuids::uuid_system_generator{};

	const auto testNode_01 = AssetNode{ uuidGenerator(), "test_subMesh_01", AssetType::subMesh, 1, "{}" };
	const auto testNode_02 = AssetNode{ uuidGenerator(), "test_subMesh_01", AssetType::subMesh, 1, "{}" };

	const auto assetFile = AssetFile{ uuidGenerator(), "test_asset_file", 1, { testNode_01, testNode_02 } };

	auto fileStream = std::ofstream(runtime_format("{}.asset", assetFile.name));
	fileStream << std::setw(4) << nlohmann::json{ assetFile } << std::endl;


	ASSERT_TRUE(true);
}

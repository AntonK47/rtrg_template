#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <MeshImporter.hpp>

using namespace Framework;

TEST(AssetImporter, LoadMissingFile)
{
	auto unitTest = testing::UnitTest::GetInstance();

	AssetImporter importer{ std::filesystem::path(unitTest->original_working_dir()) / "no_exist.obj" };
	EXPECT_FALSE(importer.HasLoadedScene());
}

TEST(AssetImporter, LoadExistingFile)
{
	auto unitTest = testing::UnitTest::GetInstance();

	AssetImporter importer{ std::filesystem::path(unitTest->original_working_dir()) / "bunny.obj" };
	EXPECT_TRUE(importer.HasLoadedScene());
}

TEST(AssetImporter, SceneLoading)
{
	auto unitTest = testing::UnitTest::GetInstance();

	AssetImporter importer{ std::filesystem::path(unitTest->original_working_dir()) / "bunny.obj" };
	const auto& sceneInformation = importer.GetSceneInformation();
	EXPECT_EQ(sceneInformation.meshCount, 1);
}

TEST(AssetImporter, ImportMeshWithDefaulStream)
{
	auto unitTest = testing::UnitTest::GetInstance();

	AssetImporter importer{ std::filesystem::path(unitTest->original_working_dir()) / "bunny.obj" };

	const auto meshData = importer.ImportMesh(0, MeshImportSettings{});

	EXPECT_TRUE(meshData.streams.empty());

	const auto settings0 = MeshImportSettings{ .verticesStreamDeclarations = {
												   VerticesStreamDeclaration{ .hasPosition = true, .hasNormal = true },
												   VerticesStreamDeclaration{ .hasTangentBitangent = true,
																			  .hasTextureCoordinate0 = true } } };
	const auto meshData0 = importer.ImportMesh(0, settings0);
	EXPECT_EQ(2, meshData0.streams.size());
}

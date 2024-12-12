#pragma once

#include "ImGuiUtils.hpp"
#include "Math.hpp"
#include "MeshImporter.hpp"

#include <Mesh.hpp>
#include <filesystem>
#include <vector>

namespace Framework
{
	namespace Editor
	{

		struct AssetImporterEditor
		{
			std::vector<std::filesystem::path> ScanAssetsInFolder()
			{
				auto foundAssetFiles = std::vector<std::filesystem::path>{};

				for (const std::filesystem::directory_entry& entry :
					 std::filesystem::recursive_directory_iterator("Assets/Meshes"))
				{
					if (entry.is_regular_file())
					{
						const auto path = entry.path();
						const auto extension = path.extension();
						if (extension == ".gltf")
						{
							foundAssetFiles.push_back(path);
						}
					}
				}
				return foundAssetFiles;
			}

			void Draw()
			{
				static auto selectedAssetFile = -1;
				static auto foundAssetFiles = std::vector<std::filesystem::path>{};
				static auto importInfo = SceneInformation{};
				ImGui::Begin("Asset Import");

				if (ImGui::Button("Scan Asset Folder"))
				{
					foundAssetFiles = ScanAssetsInFolder();
				}
				if (not foundAssetFiles.empty())
				{
					if (ImGui::BeginListBox("##assetFiles",
											ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing())))
					{
						for (int n = 0; n < foundAssetFiles.size(); n++)
						{
							bool isSelected = (selectedAssetFile == n);
							ImGuiSelectableFlags flags = (selectedAssetFile == n) ? ImGuiSelectableFlags_Highlight : 0;
							if (ImGui::Selectable(foundAssetFiles[n].generic_string().c_str(), isSelected, flags))
							{
								selectedAssetFile = n;
							}
							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndListBox();
					}
				}


				ImGui::BeginDisabled(selectedAssetFile == -1);
				if (ImGui::Button("Import"))
				{
					auto importer = Framework::AssetImporter(foundAssetFiles[selectedAssetFile]);
					importInfo = importer.GetSceneInformation();
				}
				ImGui::EndDisabled();

				ImGui::LabelText("Meshes", "%i", importInfo.meshCount);
				ImGui::LabelText("Textures", "%i", importInfo.texturesCount);
				ImGui::LabelText("Materials", "%i", importInfo.materialCount);
				ImGui::LabelText("Skeletons", "%i", importInfo.skeletonCount);
				ImGui::LabelText("Animations", "%i", importInfo.animationCount);

				ImGui::End();
			}
		};
	} // namespace Editor
} // namespace Framework
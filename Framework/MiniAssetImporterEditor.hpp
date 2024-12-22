#pragma once

#include "ImGuiUtils.hpp"
#include "Math.hpp"
#include "Mesh.hpp"
#include "MeshImporter.hpp"

#include <filesystem>
#include <vector>

#include <imgui-node-editor/imgui_node_editor.h>

namespace Framework
{
	namespace Editor
	{

		struct AssetImporterEditor
		{
			ax::NodeEditor::EditorContext* editorContext;

			AssetImporterEditor()
			{
				ax::NodeEditor::Config config;
				config.SettingsFile = "Simple.json";
				editorContext = ax::NodeEditor::CreateEditor(&config);
			}

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


				ImGui::Begin("Object Editor");

				static ImGuiTextFilter filter;
				filter.Draw();
				const auto selected = ImGui::IsItemActive();
				const char* lines[] = { "aaa1.c",	"bbb1.c",	"ccc1.c", "aaa2.cpp",
										"bbb2.cpp", "ccc2.cpp", "abc.h",  "hello, world" };

				if (selected)
				{

					if (ImGui::BeginListBox("##List"))
					{
						for (int i = 0; i < IM_ARRAYSIZE(lines); i++)
						{
							if (filter.PassFilter(lines[i]))
							{
								ImGui::Selectable(lines[i], false);
							}
						}
						ImGui::EndListBox();
					}
				}
				ImGui::End();

				ImGui::Begin("Node Test");
				ImGui::Separator();
				ax::NodeEditor::SetCurrentEditor(editorContext);
				ax::NodeEditor::Begin("editor");
				int uniqueId = 1;
				// Start drawing nodes.
				ax::NodeEditor::BeginNode(uniqueId++);
				ImGui::Text("Node A");
				ax::NodeEditor::BeginPin(uniqueId++, ax::NodeEditor::PinKind::Input);
				ImGui::Text("-> In");
				ax::NodeEditor::EndPin();
				ImGui::SameLine();
				ax::NodeEditor::BeginPin(uniqueId++, ax::NodeEditor::PinKind::Output);
				ImGui::Text("Out ->");
				ax::NodeEditor::EndPin();
				ax::NodeEditor::EndNode();
				ax::NodeEditor::End();
				ax::NodeEditor::SetCurrentEditor(nullptr);

				ImGui::End();
			}
			
			/*struct Material
			{
				int materialIndex;
			};

			struct MeshData
			{
				int offset;
			};

			struct SubMesh
			{
				MeshData* meshData{nullptr};
				Material* material{nullptr};
			};

			struct Mesh
			{
				std::vector<SubMesh*> subMeshes;
			};

			struct LodMesh
			{
				Float rangeBegin;
				Float rangeEnd;

				Mesh* mesh{nullptr};
			};

			struct Entity
			{
				Math::Matrix4x4 transform;
			};

			struct TypeReference
			{
			};

			struct TypeReferenceArray
			{
			};

			struct ExampleNode
			{
				
			};
			struct ComponentNode
			{
			};

			std::vector<ComponentNode> components;*/
		};
	} // namespace Editor
} // namespace Framework
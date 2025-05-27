#pragma once

#include "Core.hpp"
#include "Math.hpp"

#include <imgui_node_editor.h>
#include <vector>
#include <array>

namespace Framework
{
	namespace Editor
	{
		struct MaterialNode;
		struct Pin;
		struct OutputPin;
		struct InputPin;

		enum class PinType
		{
			scalarType,
			rangedScalarType,
			vector2Type,
			vector3Type,
			vector4Type,
			colorType
		};


		struct Link final
		{
			ax::NodeEditor::LinkId id{};
			ax::NodeEditor::PinId fromPinId{};
			ax::NodeEditor::PinId toPinId{};
			OutputPin* fromPin{};
			InputPin* toPin{};
			ImColor color{ 255, 255, 255, 255 };
			float thickness{ 1.0f };
		};

		struct Pin
		{
			ax::NodeEditor::PinId id{};
			MaterialNode* nodeOwner;
			ImColor color{};
			PinType type;
			std::string name{};
			std::string description{};
		};

		struct OutputPin final : Pin
		{
			U8 connectedLinksCount{ 0 };
		};

		struct InputPin final : Pin
		{
			std::vector<PinType> acceptedTypes{};
			Link* connectedLink{};
		};

		using fix_char_array = std::array<char, 64>;

		struct MaterialNodeData
		{
			Math::Vector2 position{};
			std::array<ax::NodeEditor::PinId, 16> inputPinConnections{};
			std::array<ax::NodeEditor::PinId, 16> outputPinConnections{};
		};

		struct MaterialNode
		{
			ax::NodeEditor::NodeId id{};
			fix_char_array title{};
			ImColor nodeColor{};
			std::vector<InputPin> inputPins{};
			std::vector<OutputPin> outputPins{};
			float width{ 200 };
			

			bool hasStateChanged{ false };

			inline virtual void submitState() = 0;
			inline virtual void drawNodeContent(){};
			inline virtual void drawPinContent(float maxWidth, U8 pinIndex)
			{
			}
			inline virtual void deferredDraw()
			{
			}
			inline virtual void notifyInputPinConnection()
			{
			}
			inline virtual void notifyStateChange()
			{
			}
			inline virtual std::string toString()
			{
				return std::format("[{}]", id.Get());
			}
			virtual ~MaterialNode();
		};


	} // namespace Editor
} // namespace Framework
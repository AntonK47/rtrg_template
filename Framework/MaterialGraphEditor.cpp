#include "MaterialGraphEditor.hpp"

#include <unordered_map>
#include <stack>
#include <set>

using namespace Framework;
using namespace Framework::Editor;

namespace
{
	bool hasAnyCircle(MaterialNode* root)
	{
		auto visitedNodesCount = std::unordered_map<MaterialNode*, U8>{};

		auto visitedNodes = std::stack<MaterialNode*>{};
		visitedNodes.push(root);

		while (!visitedNodes.empty())
		{
			auto node = visitedNodes.top();
			visitedNodes.pop();
			visitedNodesCount[node]++;
			if (visitedNodesCount[node] > 1)
			{
				return true;
			}

			auto connectedNodes = std::set<MaterialNode*>{};

			for (const auto& input : node->inputPins)
			{
				if (input.connectedLink)
				{
					connectedNodes.insert(input.connectedLink->fromPin->nodeOwner);
				}
			}

			for (const auto& node : connectedNodes)
			{
				visitedNodes.push(node);
			}
		}
		return false;
	}
} // namespace
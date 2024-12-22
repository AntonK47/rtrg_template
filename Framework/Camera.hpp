#pragma once

#include "Core.hpp"
#include "Math.hpp"

namespace Framework
{
	struct Camera
	{
		Math::Vector3 position;
		Math::Vector3 forward;
		Math::Vector3 up;
		Float movementSpeed{ 1.0f };
		Float movementSpeedScale{ 1.0f };
		Float sensitivity{ 1.0f };
	};
} // namespace Framework
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
		F32 movementSpeed{ 1.0f };
		F32 movementSpeedScale{ 1.0f };
		F32 sensitivity{ 1.0f };
	};
} // namespace Framework
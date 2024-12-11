#pragma once 

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace Framework
{
	struct WindowViewport
{
	uint32_t width{ 0 };
	uint32_t height{ 0 };
	bool shouldRecreateWindowSizeDependedResources{ false };

	bool IsVisible() const
	{
		return width > 0 and height > 0;
	}

	void Reset()
	{
		width = 0;
		height = 0;

		shouldRecreateWindowSizeDependedResources = true;
	}

	void UpdateSize(SDL_Window* window)
	{
		int w;
		int h;
		const auto result = SDL_GetWindowSize(window, &w, &h);

		assert(result);

		if (static_cast<uint32_t>(w) != width or static_cast<uint32_t>(h) != height)
		{
			shouldRecreateWindowSizeDependedResources = true;
		}

		width = static_cast<uint32_t>(w);
		height = static_cast<uint32_t>(h);
	}
};
}
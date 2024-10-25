#include <SDL3/SDL.h>

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("SDL_Init failed (%s)", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Template Application", 1280, 720, SDL_WINDOW_RESIZABLE);

	if (!window)
	{
		SDL_Log("SDL_CreateWindow failed (%s)", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	//DO INITIALIZATION HERE

	bool shouldRun = true;

	while (shouldRun)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				shouldRun = false;
				break;
			}
		}

		// DO RENDERING HERE
	}

	SDL_DestroyWindow(window);

	SDL_Quit();
	return 0;
}
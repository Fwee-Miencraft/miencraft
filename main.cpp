#include <iostream>
#include <SDL3/SDL.h>

int main(int argc, char* argv[])
{
    // 1. Initialize SDL (video is enough for window + basic image loading)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "SDL3 initialized successfully\n";

    // 2. Create window
    SDL_Window* window = SDL_CreateWindow(
        "SDL3 Starting Frame",
        1280,               // width
        720,                // height
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // 3. Create renderer (accelerated is fine — SDL3 handles it well)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "Window & renderer ready!\n";

    // 4. Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Clear screen (dark gray)
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_RenderClear(renderer);

        // Example drawing: blue rectangle
        SDL_SetRenderDrawColor(renderer, 80, 120, 255, 255);
        SDL_FRect rect = { 400, 200, 480, 320 };
        SDL_RenderFillRect(renderer, &rect);

        // Show the frame
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Clean exit\n";
    return 0;
}
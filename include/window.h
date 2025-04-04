#pragma once

#include <SDL2/SDL.h>
#include <string>

class Window {
    public:
        Window(const std::string &title, int width, int height);
        ~Window();

        bool Initialize();
        void RenderLoop();
        void Shutdown();
    private:
        void ProcessEvents();
        void Update();
        void Render();
        
        SDL_Window* window;
        SDL_Renderer* renderer;

        bool isRunning;
        int width;
        int height;
        std::string title;

        Uint32 lastFrameTime;
};
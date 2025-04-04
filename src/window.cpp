#include "window.h"
#include <iostream>

Window::Window(const std::string &title, int width, int height)
    : title(title), width(width), height(height), window(nullptr), renderer(nullptr), isRunning(false), lastFrameTime(0)
{
}

Window::~Window()
{
    Shutdown();
}

bool Window::Initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::cerr << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << SDL_GetError() << std::endl;
        return false;
    }

    isRunning = true;
    lastFrameTime = SDL_GetTicks();
    
    return true;
}

void Window::RenderLoop()
{
    while (isRunning) {
        ProcessEvents();
        Update();
        Render();
    }
}

void Window::Shutdown()
{
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}

void Window::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                isRunning = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    isRunning = false;
                }
                break;
            default:
                break;
        }
    }
}

void Window::Update()
{
    Uint32 currentFrameTime = SDL_GetTicks();
    Uint32 deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    if(deltaTime > 0.05f) {
        deltaTime = 0.05f;
    }
}

void Window::Render() 
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect rect = { 
        width / 4, 
        height / 4, 
        width / 2, 
        height / 2
    };
    SDL_RenderFillRect(renderer, &rect);
    
    SDL_RenderPresent(renderer);
}
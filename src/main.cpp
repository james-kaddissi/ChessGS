#include "engine.h"
#include "window.h"
#include <iostream>

int main()
{
    ChessEngine::initialize();

    Window window("Chess Game", 800, 640);
    
    if (!window.Initialize()) {
        std::cerr << "Failed to initialize window" << std::endl;
        return 1;
    }
    
    std::cout << "Chess game initialized successfully!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Click on a piece to select it" << std::endl;
    std::cout << "  - Click on a highlighted square to move the selected piece" << std::endl;
    std::cout << "  - Press 'U' to undo a move" << std::endl;
    std::cout << "  - Press 'ESC' to quit" << std::endl;
    
    window.RenderLoop();

    return 0;
}
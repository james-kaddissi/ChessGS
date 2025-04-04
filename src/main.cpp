#include "engine.h"
#include "window.h"
#include <iostream>

int main()
{
    try {
        Window window("Chess Game", 600, 600);
        bool initialized = window.Initialize();

        if (!initialized) {
            std::cerr << "Failed to initialize window." << std::endl;
            return -1;
        }
        window.RenderLoop();

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    // ChessEngine engine;

    // engine.printBoard();

    // Move move(E2, E4);

    // if (engine.makeMove(move))
    // {
    //     engine.printBoard();
    // } else {
    //     std::cout << "Issue with makeMove" << std::endl;
    // }

    // return 0;
}
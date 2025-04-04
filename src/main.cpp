#include "engine.h"
#include <iostream>

int main()
{
    ChessEngine engine;

    engine.printBoard();

    Move move(E2, E4);

    if (engine.makeMove(move))
    {
        engine.printBoard();
    } else {
        std::cout << "Issue with makeMove" << std::endl;
    }

    return 0;
}
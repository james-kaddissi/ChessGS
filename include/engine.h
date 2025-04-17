#pragma once

#include "chess_types.h"
#include "bitboard.h"
#include <string>
#include <vector>

class ChessEngine {
public:
    ChessEngine();
    ~ChessEngine();
    
    // setup operations and board
    void resetToStartingPosition();
    PieceType getPieceAt(Square sq, Color& color);
    PositionManager getPosition();
    
    // generating moves from PositionManager
    std::vector<Move> generateLegalMoves();
    bool makeMove(const Move& move);
    void unmakeMove();
    // game state
    Color getSideToMove() const;
    bool isInCheck(Color side) const;
    bool isCheckmate() const;
    bool isStalemate() const;
    
    // formatting moves
    std::string moveToString(const Move& move) const;
    
private:
    PositionManager position;
}; 
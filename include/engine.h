#pragma once

#include "chess_types.h"
#include "bitboard.h"
#include <vector>
#include <string>

class ChessEngine
{
private:
    Bitboard pieces[2][6];      // [color][piece type]
    Bitboard colorBitboards[2]; // colored piece bitboards: [WHITE, BLACK]
    Bitboard occupiedSquares;   // squares bitboards

    Color sideToMove;
    Square enPassantSquare;
    int castlingRights;
    int halfMoveClock; // for the 50 move rule
    int fullMoveNumber;

public:
    ChessEngine();
    
    void resetToStartingPosition();
    void updateCombinedBitboards();
    
    // getters for bitboards
    Bitboard getPieces(Color color, PieceType type) const;
    Bitboard getColorPieces(Color color) const;
    Bitboard getOccupiedSquares() const;
    PieceType getPieceAt(Square sq, Color &outColor) const;
    
    bool makeMove(const Move &move);
    std::vector<Move> generateMoves();
    void printBoard() const;
};
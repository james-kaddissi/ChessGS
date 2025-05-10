#pragma once

#include "chess_types.h"
#include "bitboard.h"
#include <string>
#include <vector>

struct Score {
    int mg; // midgame
    int eg; // endgame

    Score(int mg_ = 0, int eg_ = 0) : mg(mg_), eg(eg_) {}

    Score operator+(const Score& other) const {
        return Score(mg + other.mg, eg + other.eg);
    }

    Score& operator+=(const Score& other) {
        mg += other.mg;
        eg += other.eg;
        return *this;
    }

    Score operator-(const Score& other) const {
        return Score(mg - other.mg, eg - other.eg);
    }

    Score operator*(int value) const {
        return Score(mg * value, eg * value);
    }

    Score operator/(int value) const {
        return Score(mg / value, eg / value);
    }
};

class ChessEngine {
public:
    ChessEngine();
    ~ChessEngine();
    
    // setup operations and board
    void resetToStartingPosition();
    PieceType getPieceAt(Square sq, Color& color);
    
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

    // evaluation functions
    Bitboard getFriendlyPieces(Color color) const;
    int eval();
    Score evaluate_color(Color color);
    int count_material(Color color);
    int game_phase();
    Score evalPawns(Color color);
    Score evalKnights(Color color);
    Score evalBishops(Color color);
    Score evalRooks(Color color);
    Score evalQueens(Color color);
    int search(int depth, int alpha, int beta);
    Move getBestMove(int depth);
    
private:
    PositionManager position;
}; 
#pragma once

#include "chess_types.h"
#include "bitboard.h"
#include <vector>
#include <string>
#include <array>

class ChessEngine {
private:
    Bitboard pieces[2][6];
    Bitboard colorBitboards[2];
    Bitboard occupiedSquares;

    Color sideToMove;
    Square enPassantSquare;
    int castlingRights;
    int halfMoveClock;
    int fullMoveNumber;
    
    std::array<UndoInfo, 1024> history;
    int historyIndex = 0;

    bool isSquare(Square square) const;
    bool isSquareAttacked(Square square, Color attacker) const;
    
    
    Bitboard getPinnedPieces(Color kingColor) const;
    
    void addPiece(Square sq, PieceType pt, Color c);
    void removePiece(Square sq, PieceType pt, Color c);
    void movePiece(Square from, Square to, PieceType pt, Color c);
    
    std::vector<Move> generateCapturesAndPromotions() const;
    std::vector<Move> generateQuietMoves() const;

public:
    ChessEngine();
    ~ChessEngine();
    
    void resetToStartingPosition();
    
    Bitboard getPieces(Color color, PieceType type) const;
    Bitboard getColorPieces(Color color) const;
    Bitboard getOccupiedSquares() const;
    PieceType getPieceAt(Square sq, Color &outColor) const;
    
    bool makeMove(Move move);
    void unmakeMove();
    
    std::vector<Move> generateLegalMoves() const;
    std::vector<Move> generatePseudoLegalMoves() const;
    
    bool isLegalMove(Move move) const;
    bool isInCheck(Color color) const;
    void printBoard() const;
    
    std::string squareToString(Square square) const;
    std::string moveToString(Move move) const;
    Move parseMove(const std::string& moveStr) const;
    
    // Added getter for current side to move
    Color getSideToMove() const { return sideToMove; }
    
    static void initialize();
};
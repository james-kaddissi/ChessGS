#include "engine.h"
#include <iostream>

ChessEngine::ChessEngine()
{
    resetToStartingPosition();
}

void ChessEngine::resetToStartingPosition()
{
    // empty bitboards
    for (int c = 0; c < 2; ++c)
    {
        for (int p = 0; p < 6; ++p)
        {
            pieces[c][p] = 0ULL;
        }
        colorBitboards[c] = 0ULL;
    }
    occupiedSquares = 0ULL;

    // initialize pieces
    pieces[WHITE][PAWN] = 0x000000000000FF00ULL;
    pieces[WHITE][KNIGHT] = 0x0000000000000042ULL;
    pieces[WHITE][BISHOP] = 0x0000000000000024ULL;
    pieces[WHITE][ROOK] = 0x0000000000000081ULL;
    pieces[WHITE][QUEEN] = 0x0000000000000008ULL;
    pieces[WHITE][KING] = 0x0000000000000010ULL;

    pieces[BLACK][PAWN] = 0x00FF000000000000ULL;  
    pieces[BLACK][KNIGHT] = 0x4200000000000000ULL;
    pieces[BLACK][BISHOP] = 0x2400000000000000ULL;
    pieces[BLACK][ROOK] = 0x8100000000000000ULL;
    pieces[BLACK][QUEEN] = 0x0800000000000000ULL;
    pieces[BLACK][KING] = 0x1000000000000000ULL;

    updateCombinedBitboards();

    // initialize game state
    sideToMove = WHITE;
    enPassantSquare = A1;
    castlingRights = WHITE_KINGSIDE | WHITE_QUEENSIDE | BLACK_KINGSIDE | BLACK_QUEENSIDE;
    halfMoveClock = 0;
    fullMoveNumber = 1;
}

void ChessEngine::updateCombinedBitboards()
{
    colorBitboards[WHITE] = 0ULL;
    colorBitboards[BLACK] = 0ULL;

    for (int p = 0; p < 6; ++p)
    {
        colorBitboards[WHITE] |= pieces[WHITE][p];
        colorBitboards[BLACK] |= pieces[BLACK][p];
    }

    occupiedSquares = colorBitboards[WHITE] | colorBitboards[BLACK];
}

Bitboard ChessEngine::getPieces(Color color, PieceType type) const
{
    return pieces[color][type];
}

Bitboard ChessEngine::getColorPieces(Color color) const
{
    return colorBitboards[color];
}

Bitboard ChessEngine::getOccupiedSquares() const
{
    return occupiedSquares;
}

PieceType ChessEngine::getPieceAt(Square sq, Color &outColor) const
{
    Bitboard sqBitboard = squareToBitboard(sq);

    if (!(occupiedSquares & sqBitboard))
    {
        outColor = WHITE;
        return NONE;
    }

    if (colorBitboards[WHITE] & sqBitboard)
    {
        outColor = WHITE;
    }
    else
    {
        outColor = BLACK;
    }

    for (int p = 0; p < 6; ++p)
    {
        if (pieces[outColor][p] & sqBitboard)
        {
            return static_cast<PieceType>(p);
        }
    }

    return NONE;
}

bool ChessEngine::makeMove(const Move &move)
{
    Square from = move.from;
    Square to = move.to;

    Color pieceColor;
    PieceType pieceType = getPieceAt(from, pieceColor);

    if (pieceType == NONE || pieceColor != sideToMove)
    {
        return false;
    }

    Bitboard fromBB = squareToBitboard(from);
    Bitboard toBB = squareToBitboard(to);

    pieces[pieceColor][pieceType] &= ~fromBB;

    Color capturedColor;
    PieceType capturedPiece = getPieceAt(to, capturedColor);

    if (capturedPiece != NONE)
    {
        pieces[capturedColor][capturedPiece] &= ~toBB;
    }

    pieces[pieceColor][pieceType] |= toBB;

    updateCombinedBitboards();

    sideToMove = (sideToMove == WHITE) ? BLACK : WHITE;

    // DOES NOT handle en passant, promotion, castling, or half-move clock increments yet

    return true;
}

std::vector<Move> ChessEngine::generateMoves()
{
    std::vector<Move> moves;

    // just a setup for generating moves for the current side to move

    return moves;
}

void ChessEngine::printBoard() const
{
    std::cout << "  a b c d e f g h" << std::endl;
    for (int rank = 7; rank >= 0; --rank)
    {
        std::cout << rank + 1 << " ";
        for (int file = 0; file < 8; ++file)
        {
            Square sq = static_cast<Square>(rank * 8 + file);
            Color pieceColor;
            PieceType pieceType = getPieceAt(sq, pieceColor);

            char pieceChar = '.';
            if (pieceType != NONE)
            {
                const char *pieces = "pnbrqk";
                pieceChar = pieces[pieceType];
                if (pieceColor == WHITE)
                {
                    pieceChar = std::toupper(pieceChar);
                }
            }
            std::cout << pieceChar << " ";
        }
        std::cout << rank + 1 << std::endl;
    }
    std::cout << "  a b c d e f g h" << std::endl;

    std::cout << "Side to move: " << (sideToMove == WHITE ? "White" : "Black") << std::endl;
    std::cout << "Castling rights: ";
    if (castlingRights & WHITE_KINGSIDE)
        std::cout << "K";
    if (castlingRights & WHITE_QUEENSIDE)
        std::cout << "Q";
    if (castlingRights & BLACK_KINGSIDE)
        std::cout << "k";
    if (castlingRights & BLACK_QUEENSIDE)
        std::cout << "q";
    if (castlingRights == 0)
        std::cout << "-";
    std::cout << std::endl;
}
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

// a bitboard is a 64-bit unsigned integer where each bit represents a square on the chessboard
using Bitboard = uint64_t;

enum PieceType
{
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONE
};

enum Color
{
    WHITE,
    BLACK
};

enum Square
{
    A1,
    B1,
    C1,
    D1,
    E1,
    F1,
    G1,
    H1,
    A2,
    B2,
    C2,
    D2,
    E2,
    F2,
    G2,
    H2,
    A3,
    B3,
    C3,
    D3,
    E3,
    F3,
    G3,
    H3,
    A4,
    B4,
    C4,
    D4,
    E4,
    F4,
    G4,
    H4,
    A5,
    B5,
    C5,
    D5,
    E5,
    F5,
    G5,
    H5,
    A6,
    B6,
    C6,
    D6,
    E6,
    F6,
    G6,
    H6,
    A7,
    B7,
    C7,
    D7,
    E7,
    F7,
    G7,
    H7,
    A8,
    B8,
    C8,
    D8,
    E8,
    F8,
    G8,
    H8
};

enum CastlingRights
{
    WHITE_KINGSIDE = 1,
    WHITE_QUEENSIDE = 2,
    BLACK_KINGSIDE = 4,
    BLACK_QUEENSIDE = 8
};

struct Move
{
    Square from;
    Square to;
    PieceType promotion;

    Move(Square f, Square t, PieceType p = NONE) : from(f), to(t), promotion(p) {}
};

class ChessEngine
{
private:
    Bitboard pieces[2][6];      // [color][piece type]
    Bitboard colorBitboards[2]; // colored piece bitboards: [WHITE, BLACK]
    Bitboard occupiedSquares;   // squares bitboards

    Color sideToMove;
    Square enPassantSquare;
    int castlingRights;
    int halfMoveClock;
    int fullMoveNumber;

    // bitboard operations

    Bitboard squareToBitboard(Square sq) const
    {
        return 1ULL << sq;
    }

    Square lsb(Bitboard b) const
    {
        if (b == 0)
            return A1; // empty bitboard

        return static_cast<Square>(__builtin_ctzll(b));
    }

    Bitboard lsbPop(Bitboard &b)
    {
        Bitboard result = b & -b; // get lsb
        b &= (b - 1);             // clear the lsb
        return result;
    }

    void printBitboard(Bitboard b) const
    {
        // format as 8x8
        for (int rank = 7; rank >= 0; --rank)
        {
            for (int file = 0; file < 8; ++file)
            {
                Square sq = static_cast<Square>(rank * 8 + file);
                std::cout << ((b & squareToBitboard(sq)) ? '1' : '0') << ' ';
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

public:
    ChessEngine()
    {
        resetToStartingPosition();
    }

    void resetToStartingPosition()
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

    void updateCombinedBitboards()
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

    // getters for bitboards
    Bitboard getPieces(Color color, PieceType type) const
    {
        return pieces[color][type];
    }

    Bitboard getColorPieces(Color color) const
    {
        return colorBitboards[color];
    }

    Bitboard getOccupiedSquares() const
    {
        return occupiedSquares;
    }

    // returns the piece type and sets the color of the input pointer
    PieceType getPieceAt(Square sq, Color &outColor) const
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

    bool makeMove(const Move &move)
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

    std::vector<Move> generateMoves()
    {
        std::vector<Move> moves;

        // just a setup for generating moves for the current side to move

        return moves;
    }

    void printBoard() const
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
};

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
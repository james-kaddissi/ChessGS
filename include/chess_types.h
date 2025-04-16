#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

// a bitboard is a 64-bit unsigned integer where each bit represents a square on the chessboard
using Bitboard = uint64_t;
using Move = uint16_t;

enum PieceType {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE
};

enum Color {
    WHITE, BLACK
};

enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQ
};

inline Square& operator++(Square& sq) {
    return sq = static_cast<Square>(static_cast<int>(sq) + 1);
}

inline Square operator++(Square& sq, int) {
    Square old = sq;
    sq = static_cast<Square>(static_cast<int>(sq) + 1);
    return old;
}

inline int squareDistance(Square s1, Square s2) {
    int file1 = s1 & 7, file2 = s2 & 7;
    int rank1 = s1 >> 3, rank2 = s2 >> 3;
    return std::max(std::abs(file1 - file2), std::abs(rank1 - rank2));
}

enum MoveFlag {
    QUIET = 0,
    DOUBLE_PAWN_PUSH = 1,
    KING_CASTLE = 2, 
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    PROMOTION_KNIGHT = 8,
    PROMOTION_BISHOP = 9,
    PROMOTION_ROOK = 10,
    PROMOTION_QUEEN = 11,
    PROMOTION_KNIGHT_CAPTURE = 12,
    PROMOTION_BISHOP_CAPTURE = 13,
    PROMOTION_ROOK_CAPTURE = 14,
    PROMOTION_QUEEN_CAPTURE = 15
};

constexpr Move createMove(Square from, Square to, MoveFlag flag = QUIET) {
    return static_cast<Move>(from | (to << 6) | (flag << 12));
}

constexpr Square getFrom(Move m) { return static_cast<Square>(m & 0x3F); }
constexpr Square getTo(Move m) { return static_cast<Square>((m >> 6) & 0x3F); }
constexpr MoveFlag getFlag(Move m) { return static_cast<MoveFlag>((m >> 12) & 0xF); }
constexpr bool isPromotion(Move m) { return (m >> 12) & 0x8; }
constexpr bool isCapture(Move m) { return (m >> 12) & 0x4; }
constexpr PieceType PromotionPieces[8] = {NONE, NONE, NONE, NONE, KNIGHT, BISHOP, ROOK, QUEEN};

constexpr PieceType getPromotionType(Move m) {
    return PromotionPieces[(m >> 12) & 0x7];
}

enum CastlingRights {
    WHITE_KINGSIDE = 1,
    WHITE_QUEENSIDE = 2,
    BLACK_KINGSIDE = 4,
    BLACK_QUEENSIDE = 8
};

const Bitboard RANK_1 = 0x00000000000000FFULL;
const Bitboard RANK_2 = 0x000000000000FF00ULL;
const Bitboard RANK_3 = 0x0000000000FF0000ULL;
const Bitboard RANK_4 = 0x00000000FF000000ULL;
const Bitboard RANK_5 = 0x000000FF00000000ULL;
const Bitboard RANK_6 = 0x0000FF0000000000ULL;
const Bitboard RANK_7 = 0x00FF000000000000ULL;
const Bitboard RANK_8 = 0xFF00000000000000ULL;

const Bitboard FILE_A = 0x0101010101010101ULL;
const Bitboard FILE_B = 0x0202020202020202ULL;
const Bitboard FILE_C = 0x0404040404040404ULL;
const Bitboard FILE_D = 0x0808080808080808ULL;
const Bitboard FILE_E = 0x1010101010101010ULL;
const Bitboard FILE_F = 0x2020202020202020ULL;
const Bitboard FILE_G = 0x4040404040404040ULL;
const Bitboard FILE_H = 0x8080808080808080ULL;

const Bitboard NOT_FILE_A = ~FILE_A;
const Bitboard NOT_FILE_H = ~FILE_H;
const Bitboard NOT_FILE_AB = ~(FILE_A | FILE_B);
const Bitboard NOT_FILE_GH = ~(FILE_G | FILE_H);

const int NORTH = 8;
const int EAST = 1;
const int SOUTH = -8;
const int WEST = -1;
const int NORTH_EAST = 9;
const int SOUTH_EAST = -7;
const int SOUTH_WEST = -9;
const int NORTH_WEST = 7;

const int LEGAL_KNIGHT_MOVES[8] = {
    NORTH + NORTH + EAST,
    NORTH + NORTH + WEST,
    SOUTH + SOUTH + EAST,
    SOUTH + SOUTH + WEST,
    EAST + EAST + NORTH,
    EAST + EAST + SOUTH,
    WEST + WEST + NORTH,
    WEST + WEST + SOUTH
};

const int LEGAL_KING_MOVES[8] = {
    NORTH, NORTH_EAST, EAST, SOUTH_EAST,
    SOUTH, SOUTH_WEST, WEST, NORTH_WEST
};

const Bitboard startingPieces[2][6] = {
    {
        0x000000000000FF00ULL,
        0x0000000000000042ULL,  
        0x0000000000000024ULL,
        0x0000000000000081ULL,
        0x0000000000000008ULL,
        0x0000000000000010ULL
    },
    {
        0x00FF000000000000ULL,
        0x4200000000000000ULL,
        0x2400000000000000ULL,
        0x8100000000000000ULL,
        0x0800000000000000ULL,
        0x1000000000000000ULL
    }
};

struct UndoInfo {
    Move move;
    int castlingRights;
    Square enPassantSquare;
    int halfMoveClock;
    PieceType capturedPiece;
    bool isNullMove;
};

extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];
extern Bitboard PawnAttacks[2][64];
extern Bitboard BetweenSquares[64][64];
extern Bitboard LineSquares[64][64];

extern Bitboard MagicRookMasks[64];
extern Bitboard MagicBishopMasks[64];
extern uint64_t MagicRookNumbers[64];
extern uint64_t MagicBishopNumbers[64];
extern int MagicRookShifts[64];
extern int MagicBishopShifts[64];
extern Bitboard* MagicRookAttacks[64];
extern Bitboard* MagicBishopAttacks[64];
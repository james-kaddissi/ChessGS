#pragma once

#include "chess_types.h"
#include <iostream>

inline Bitboard squareToBitboard(Square sq) {
    return 1ULL << sq;
}

inline Square bitboardToSquare(Bitboard bb) {
    return static_cast<Square>(__builtin_ctzll(bb));
}

inline int popCount(Bitboard b) {
    return __builtin_popcountll(b);
}

inline Square lsb(Bitboard b) {
    if (b == 0) return NO_SQ;
    return static_cast<Square>(__builtin_ctzll(b));
}

inline Bitboard lsbPop(Bitboard &b) {
    Bitboard result = b & -b;
    b &= (b - 1);
    return result;
}

inline int getRank(Square sq) { 
    return sq >> 3; 
}

inline int getFile(Square sq) { 
    return sq & 7; 
}

inline bool isSquare(Square sq) {
    return sq >= A1 && sq <= H8;
}

inline void printBitboard(Bitboard b) {
    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            Square sq = static_cast<Square>(rank * 8 + file);
            std::cout << ((b & squareToBitboard(sq)) ? '1' : '0') << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

inline Bitboard shiftNorth(Bitboard b) { return b << 8; }
inline Bitboard shiftSouth(Bitboard b) { return b >> 8; }
inline Bitboard shiftEast(Bitboard b) { return (b << 1) & NOT_FILE_A; }
inline Bitboard shiftWest(Bitboard b) { return (b >> 1) & NOT_FILE_H; }
inline Bitboard shiftNorthEast(Bitboard b) { return (b << 9) & NOT_FILE_A; }
inline Bitboard shiftNorthWest(Bitboard b) { return (b << 7) & NOT_FILE_H; }
inline Bitboard shiftSouthEast(Bitboard b) { return (b >> 7) & NOT_FILE_A; }
inline Bitboard shiftSouthWest(Bitboard b) { return (b >> 9) & NOT_FILE_H; }

inline Bitboard getBishopAttacks(Square sq, Bitboard occupied) {
    Bitboard mask = MagicBishopMasks[sq];
    uint64_t index = ((occupied & mask) * MagicBishopNumbers[sq]) >> MagicBishopShifts[sq];
    return MagicBishopAttacks[sq][index];
}

inline Bitboard getRookAttacks(Square sq, Bitboard occupied) {
    Bitboard mask = MagicRookMasks[sq];
    uint64_t index = ((occupied & mask) * MagicRookNumbers[sq]) >> MagicRookShifts[sq];
    return MagicRookAttacks[sq][index];
}

inline Bitboard getQueenAttacks(Square sq, Bitboard occupied) {
    return getBishopAttacks(sq, occupied) | getRookAttacks(sq, occupied);
}

void initAttackTables();
void initMagicBitboards();
void initBetweenAndLineSquares();
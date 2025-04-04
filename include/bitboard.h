#pragma once

#include "chess_types.h"
#include <iostream>

inline Bitboard squareToBitboard(Square sq)
{
    return 1ULL << sq;
}

inline Square lsb(Bitboard b)
{
    if (b == 0)
        return A1; // empty bitboard

    return static_cast<Square>(__builtin_ctzll(b));
}

inline Bitboard lsbPop(Bitboard &b)
{
    Bitboard result = b & -b; // get lsb
    b &= (b - 1);             // clear the lsb
    return result;
}

inline void printBitboard(Bitboard b)
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
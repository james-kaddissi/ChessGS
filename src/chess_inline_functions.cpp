#include "chess_types.h"
#include "lookup_tables.h"

int pop_count(Bitboard x) {
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    x = (x * 0x0101010101010101ULL) >> 56;
    return static_cast<int>(x);
}

int sparse_pop_count(Bitboard x) {
    int count = 0;
    while (x) {
        count++;
        x &= x - 1;
    }
    return count;
}

Square pop_lsb(Bitboard* b) {
    int lsb = bsf(*b);
    *b &= *b - 1;
    return Square(lsb);
}

Square bsf(Bitboard b) {
    return Square(DEBRUIJN64[((b ^ (b - 1)) * MAGIC) >> 58]);
}

Bitboard get_rook_attacks(Square square, Bitboard occ) {
    return ROOK_ATTACKS[square][((occ & ROOK_ATTACK_MASKS[square]) * ROOK_MAGICS[square])
        >> ROOK_ATTACK_SHIFTS[square]];
}

Bitboard get_bishop_attacks(Square square, Bitboard occ) {
    return BISHOP_ATTACKS[square][((occ & BISHOP_ATTACK_MASKS[square]) * BISHOP_MAGICS[square])
        >> BISHOP_ATTACK_SHIFTS[square]];
} 
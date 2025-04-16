#include "bitboard.h"
#include <random>
#include <algorithm>
#include <cstring>

Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];
Bitboard PawnAttacks[2][64];
Bitboard BetweenSquares[64][64];
Bitboard LineSquares[64][64];

Bitboard MagicRookMasks[64];
Bitboard MagicBishopMasks[64];
uint64_t MagicRookNumbers[64];
uint64_t MagicBishopNumbers[64];
int MagicRookShifts[64];
int MagicBishopShifts[64];
Bitboard* MagicRookAttacks[64];
Bitboard* MagicBishopAttacks[64];

const uint64_t RookMagics[64] = {
    0x8a80104000800020ULL, 0x140002000100040ULL, 0x2801880a0017001ULL, 0x100032000000080ULL,
    0x124080204001001ULL, 0x1080100008000ULL, 0x200020014008040ULL, 0x800080004000200ULL,
    0x1000200040010ULL, 0x2800800200040080ULL, 0x100080080008000ULL, 0x800200040005000ULL,
    0x208020001000080ULL, 0x4000200040100100ULL, 0x80020004008080ULL, 0x800100020008080ULL,
    0x8000404004010200ULL, 0x20002010008ULL, 0x80802008040ULL, 0x100808010000ULL,
    0x8080008000400020ULL, 0x200020100040ULL, 0x500040008008080ULL, 0x400020080800400ULL,
    0x1014000200040000ULL, 0x2000001004800ULL, 0x20001004084ULL, 0x802080004000ULL,
    0x20880000100200ULL, 0x100080200040080ULL, 0x20040100100ULL, 0x404010002000ULL,
    0x8010001000ULL, 0x120400400200ULL, 0x200100200400ULL, 0x10080800100ULL,
    0x8000400020080ULL, 0x1000200010004ULL, 0x40201000800ULL, 0x21000300ULL,
    0x20000050004ULL, 0x400080008020ULL, 0x400010090ULL, 0x1008010000200ULL,
    0x801000500040ULL, 0x1000080080200ULL, 0x8200004010ULL, 0x4040001000ULL,
    0x48002000100ULL, 0x102082000ULL, 0x2000280014000ULL, 0x880020000800ULL,
    0x4000080800100ULL, 0x20001100ULL, 0x1001010000ULL, 0x802001008080ULL,
    0x100004080ULL, 0x1250108010010ULL, 0x1040200820004ULL, 0x210004000400ULL,
    0x802008010100ULL, 0x401004080200ULL, 0x40010010000ULL, 0x1000800040005ULL
};

const uint64_t BishopMagics[64] = {
    0x40040844404084ULL, 0x2004208a004208ULL, 0x10190041080202ULL, 0x108060845042010ULL,
    0x581104180800210ULL, 0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050404440404ULL, 0x21001420088ULL, 0x24d0080801082102ULL, 0x1020a0a020400ULL,
    0x40308200402ULL, 0x4011002100800ULL, 0x401484104104005ULL, 0x801010402020200ULL,
    0x400210c3880100ULL, 0x404022024108200ULL, 0x810018200204102ULL, 0x4002801a02003ULL,
    0x85040820080400ULL, 0x810102c808880400ULL, 0xe900410884800ULL, 0x8002020480840102ULL,
    0x220200865090201ULL, 0x2010100a02021202ULL, 0x152048408022401ULL, 0x20080002081110ULL,
    0x4001001021004000ULL, 0x800040400a011002ULL, 0xe4004081011002ULL, 0x1c004001012080ULL,
    0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
    0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
    0x209188240001000ULL, 0x400408a884001800ULL, 0x110400a6080400ULL, 0x1840060a44020800ULL,
    0x90080104000041ULL, 0x201011000808101ULL, 0x1a2208080504f080ULL, 0x8012020600211212ULL,
    0x500861011240000ULL, 0x180806108200800ULL, 0x4000020e01040044ULL, 0x300000261044000aULL,
    0x802241102020002ULL, 0x20906061210001ULL, 0x5a84841004010310ULL, 0x4010801011c04ULL,
    0xa010109502200ULL, 0x4a02012000ULL, 0x500201010098b028ULL, 0x8040002811040900ULL,
    0x28000010020204ULL, 0x6000020202d0240ULL, 0x8918844842082200ULL, 0x4010011029020020ULL
};

Bitboard generateRayAttacks(Square sq, int direction, Bitboard occupancy) {
    Bitboard attacks = 0ULL;
    int r = getRank(sq);
    int f = getFile(sq);
    int dr = direction == NORTH ? 1 : (direction == SOUTH ? -1 : 0);
    int df = direction == EAST ? 1 : (direction == WEST ? -1 : 0);
    
    if (direction == NORTH_EAST || direction == SOUTH_EAST || direction == SOUTH_WEST || direction == NORTH_WEST) {
        dr = direction == NORTH_EAST || direction == NORTH_WEST ? 1 : -1;
        df = direction == NORTH_EAST || direction == SOUTH_EAST ? 1 : -1;
    }
    
    for (int i = 1; i < 8; i++) {
        int newRank = r + i * dr;
        int newFile = f + i * df;
        
        if (newRank < 0 || newRank >= 8 || newFile < 0 || newFile >= 8)
            break;
        
        Square newSq = static_cast<Square>(newRank * 8 + newFile);
        attacks |= squareToBitboard(newSq);
        
        if (squareToBitboard(newSq) & occupancy)
            break;
    }
    
    return attacks;
}

Bitboard generateSliderAttacks(Square sq, Bitboard occupancy, bool bishop) {
    Bitboard attacks = 0ULL;
    const int* directions;
    int numDirections;
    
    if (bishop) {
        static const int bishopDirections[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};
        directions = bishopDirections;
        numDirections = 4;
    } else {
        static const int rookDirections[4] = {NORTH, EAST, SOUTH, WEST};
        directions = rookDirections;
        numDirections = 4;
    }
    
    for (int i = 0; i < numDirections; i++) {
        attacks |= generateRayAttacks(sq, directions[i], occupancy);
    }
    
    return attacks;
}

Bitboard createRookMask(Square sq) {
    Bitboard mask = 0ULL;
    int rank = getRank(sq);
    int file = getFile(sq);
    
    for (int r = rank + 1; r < 7; r++)
        mask |= squareToBitboard(static_cast<Square>(r * 8 + file));
    for (int r = rank - 1; r > 0; r--)
        mask |= squareToBitboard(static_cast<Square>(r * 8 + file));
    for (int f = file + 1; f < 7; f++)
        mask |= squareToBitboard(static_cast<Square>(rank * 8 + f));
    for (int f = file - 1; f > 0; f--)
        mask |= squareToBitboard(static_cast<Square>(rank * 8 + f));
    
    return mask;
}

Bitboard createBishopMask(Square sq) {
    Bitboard mask = 0ULL;
    int rank = getRank(sq);
    int file = getFile(sq);
    
    for (int r = rank + 1, f = file + 1; r < 7 && f < 7; r++, f++)
        mask |= squareToBitboard(static_cast<Square>(r * 8 + f));
    for (int r = rank + 1, f = file - 1; r < 7 && f > 0; r++, f--)
        mask |= squareToBitboard(static_cast<Square>(r * 8 + f));
    for (int r = rank - 1, f = file + 1; r > 0 && f < 7; r--, f++)
        mask |= squareToBitboard(static_cast<Square>(r * 8 + f));
    for (int r = rank - 1, f = file - 1; r > 0 && f > 0; r--, f--)
        mask |= squareToBitboard(static_cast<Square>(r * 8 + f));
    
    return mask;
}

void initAttackTables() {
    for (Square sq = A1; sq <= H8; sq++) {
        KnightAttacks[sq] = 0ULL;
        for (int i = 0; i < 8; i++) {
            Square to = static_cast<Square>(sq + LEGAL_KNIGHT_MOVES[i]);
            if (isSquare(to) && squareDistance(sq, to) <= 2)
                KnightAttacks[sq] |= squareToBitboard(to);
        }
    }
    
    for (Square sq = A1; sq <= H8; sq++) {
        KingAttacks[sq] = 0ULL;
        for (int i = 0; i < 8; i++) {
            Square to = static_cast<Square>(sq + LEGAL_KING_MOVES[i]);
            if (isSquare(to) && squareDistance(sq, to) == 1)
                KingAttacks[sq] |= squareToBitboard(to);
        }
    }
    
    for (Square sq = A1; sq <= H8; sq++) {
        PawnAttacks[WHITE][sq] = 0ULL;
        PawnAttacks[BLACK][sq] = 0ULL;
        
        Bitboard b = squareToBitboard(sq);
        
        if (getFile(sq) > 0) {
            PawnAttacks[WHITE][sq] |= (b << 7);
            PawnAttacks[BLACK][sq] |= (b >> 9);
        }
        
        if (getFile(sq) < 7) {
            PawnAttacks[WHITE][sq] |= (b << 9);
            PawnAttacks[BLACK][sq] |= (b >> 7);
        }
    }
}

void initBetweenAndLineSquares() {
    for (Square s1 = A1; s1 <= H8; s1++) {
        for (Square s2 = A1; s2 <= H8; s2++) {
            BetweenSquares[s1][s2] = 0ULL;
            LineSquares[s1][s2] = 0ULL;
            
            if (s1 == s2) continue;
            
            int rankDiff = getRank(s2) - getRank(s1);
            int fileDiff = getFile(s2) - getFile(s1);
            
            if (rankDiff == 0 || fileDiff == 0 || std::abs(rankDiff) == std::abs(fileDiff)) {
                LineSquares[s1][s2] = 0ULL;
                BetweenSquares[s1][s2] = 0ULL;
                
                int rankDir = (rankDiff == 0) ? 0 : (rankDiff > 0 ? 1 : -1);
                int fileDir = (fileDiff == 0) ? 0 : (fileDiff > 0 ? 1 : -1);
                
                Square s = s1;
                do {
                    LineSquares[s1][s2] |= squareToBitboard(s);
                    s = static_cast<Square>(s + rankDir * 8 + fileDir);
                } while (s != s2 && isSquare(s));
                
                LineSquares[s1][s2] |= squareToBitboard(s2);
                
                s = s1;
                s = static_cast<Square>(s + rankDir * 8 + fileDir);
                while (s != s2 && isSquare(s)) {
                    BetweenSquares[s1][s2] |= squareToBitboard(s);
                    s = static_cast<Square>(s + rankDir * 8 + fileDir);
                }
            }
        }
    }
}

void initMagicBitboards() {
    for (Square sq = A1; sq <= H8; sq++) {
        MagicRookMasks[sq] = createRookMask(sq);
        int rookBits = popCount(MagicRookMasks[sq]);
        MagicRookShifts[sq] = 64 - rookBits;
        MagicRookNumbers[sq] = RookMagics[sq];
        MagicRookAttacks[sq] = new Bitboard[1 << rookBits];
        
        Bitboard rookSubset = 0ULL;
        do {
            uint64_t magic_index = ((rookSubset & MagicRookMasks[sq]) * MagicRookNumbers[sq]) >> MagicRookShifts[sq];
            MagicRookAttacks[sq][magic_index] = generateSliderAttacks(sq, rookSubset, false);
            
            rookSubset = (rookSubset - MagicRookMasks[sq]) & MagicRookMasks[sq];
        } while (rookSubset);
        
        MagicBishopMasks[sq] = createBishopMask(sq);
        int bishopBits = popCount(MagicBishopMasks[sq]);
        MagicBishopShifts[sq] = 64 - bishopBits;
        MagicBishopNumbers[sq] = BishopMagics[sq];
        MagicBishopAttacks[sq] = new Bitboard[1 << bishopBits];
        
        Bitboard bishopSubset = 0ULL;
        do {
            uint64_t magic_index = ((bishopSubset & MagicBishopMasks[sq]) * MagicBishopNumbers[sq]) >> MagicBishopShifts[sq];
            MagicBishopAttacks[sq][magic_index] = generateSliderAttacks(sq, bishopSubset, true);
            
            bishopSubset = (bishopSubset - MagicBishopMasks[sq]) & MagicBishopMasks[sq];
        } while (bishopSubset);
    }
}
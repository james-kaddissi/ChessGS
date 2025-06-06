#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <ostream>
#include <vector>

// a bitboard is a 64-bit unsigned integer where each bit represents a square on the chessboard
typedef uint64_t Bitboard;

const size_t NCOLORS = 2;
enum Color : int {
    WHITE, BLACK
};

constexpr Color operator~(Color c) {
    return Color(c ^ BLACK);
}

const size_t NDIRS = 8;
enum Direction : int {
	NORTH = 8, NORTH_EAST = 9, EAST = 1, SOUTH_EAST = -7,
	SOUTH = -8, SOUTH_WEST = -9, WEST = -1, NORTH_WEST = 7,
	NORTH_NORTH = 16, SOUTH_SOUTH = -16
};

const size_t NPIECE_TYPES = 6;
enum PieceType : int {
	PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE
};

const std::string PIECE_STR = "PNBRQK~>pnbrqk.";
const std::string DEFAULT_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
const std::string KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";

const size_t NPIECES = 15;

enum Piece : int {
	WHITE_PAWN, 
    WHITE_KNIGHT, 
    WHITE_BISHOP, 
    WHITE_ROOK, 
    WHITE_QUEEN, 
    WHITE_KING,
	BLACK_PAWN = 8, 
    BLACK_KNIGHT,
    BLACK_BISHOP, 
    BLACK_ROOK, 
    BLACK_QUEEN, 
    BLACK_KING,
	NO_PIECE
};

constexpr Piece make_piece(Color c, PieceType pt) {
	return Piece((c << 3) + pt);
}

constexpr PieceType piece_type(Piece p) {
	return PieceType(p & 0b111);
}

constexpr Color piece_color(Piece p) {
	return Color((p & 0b1000) >> 3);
}

const size_t NSQUARES = 64;
enum Square : int {
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

inline Square& operator++(Square& s) { return s = Square(int(s) + 1); }
constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
inline Square& operator-=(Square& s, Direction d) { return s = s - d; }

enum File : int {
	AFILE, BFILE, CFILE, DFILE, EFILE, FFILE, GFILE, HFILE
};	

enum Rank : int {
	RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8
};

extern const char* SQUARE_STR[65];
extern const Bitboard MASK_FILE[8];
extern const Bitboard MASK_RANK[8];
extern const Bitboard MASK_DIAGONAL[15];
extern const Bitboard MASK_ANTI_DIAGONAL[15];
extern const Bitboard SQUARE_BB[65];

extern void print_bitboard(Bitboard b);

extern const Bitboard k1;
extern const Bitboard k2;
extern const Bitboard k4;
extern const Bitboard kf;

extern int pop_count(Bitboard x);
extern int sparse_pop_count(Bitboard x);
extern Square pop_lsb(Bitboard* b);

extern const int DEBRUIJN64[64];
extern const Bitboard MAGIC;
extern Square bsf(Bitboard b);

constexpr Rank rank_of(Square s) { return Rank(s >> 3); }
constexpr File file_of(Square s) { return File(s & 0b111); }
constexpr int diagonal_of(Square s) { return 7 + rank_of(s) - file_of(s); }
constexpr int anti_diagonal_of(Square s) { return rank_of(s) + file_of(s); }
constexpr Square create_square(File f, Rank r) { return Square(r << 3 | f); }

template<Direction D>
constexpr Bitboard shift(Bitboard b) {
	return D == NORTH ? b << 8 : D == SOUTH ? b >> 8
		: D == NORTH + NORTH ? b << 16 : D == SOUTH + SOUTH ? b >> 16
		: D == EAST ? (b & ~MASK_FILE[HFILE]) << 1 : D == WEST ? (b & ~MASK_FILE[AFILE]) >> 1
		: D == NORTH_EAST ? (b & ~MASK_FILE[HFILE]) << 9 
		: D == NORTH_WEST ? (b & ~MASK_FILE[AFILE]) << 7
		: D == SOUTH_EAST ? (b & ~MASK_FILE[HFILE]) >> 7 
		: D == SOUTH_WEST ? (b & ~MASK_FILE[AFILE]) >> 9
		: 0;	
}

template<Color C>
constexpr Rank relative_rank(Rank r) {
	return C == WHITE ? r : Rank(RANK8 - r);
}

template<Color C>
constexpr Direction relative_dir(Direction d) {
	return Direction(C == WHITE ? d : -d);
}

enum MoveFlags : int {
	QUIET = 0b0000, 
    DOUBLE_PUSH = 0b0001,
	OO = 0b0010, 
    OOO = 0b0011,
	CAPTURE = 0b1000,
	CAPTURES = 0b1111,
	EN_PASSANT = 0b1010,
	PROMOTIONS = 0b0111,
	PROMOTION_CAPTURES = 0b1100,
	PR_KNIGHT = 0b0100, 
    PR_BISHOP = 0b0101, 
    PR_ROOK = 0b0110, 
    PR_QUEEN = 0b0111,
	PC_KNIGHT = 0b1100, 
    PC_BISHOP = 0b1101, 
    PC_ROOK = 0b1110, 
    PC_QUEEN = 0b1111,
};

class Move {
private:
	uint16_t move;
public:
	inline Move() : move(0) {}
	
	inline Move(uint16_t m) { move = m; }

	inline Move(Square from, Square to) : move(0) {
		move = (from << 6) | to;
	}

	inline Move(Square from, Square to, MoveFlags flags) : move(0) {
		move = (flags << 12) | (from << 6) | to;
	}

	Move(const std::string& move) {
		this->move = (create_square(File(move[0] - 'a'), Rank(move[1] - '1')) << 6) |
			create_square(File(move[2] - 'a'), Rank(move[3] - '1'));
	}

	inline Square to() const { return Square(move & 0x3f); }
	inline Square from() const { return Square((move >> 6) & 0x3f); }
	inline int to_from() const { return move & 0xffff; }
	inline MoveFlags flags() const { return MoveFlags((move >> 12) & 0xf); }

	inline bool is_capture() const {
		return (move >> 12) & CAPTURES;
	}

	void operator=(Move m) { move = m.move; }
	bool operator==(Move a) const { return to_from() == a.to_from(); }
	bool operator!=(Move a) const { return to_from() != a.to_from(); }
};

template<MoveFlags F = QUIET>
inline Move *make(Square from, Bitboard to, Move *list) {
	while (to) *list++ = Move(from, pop_lsb(&to), F);
	return list;
}

template<>
inline Move *make<PROMOTIONS>(Square from, Bitboard to, Move *list) {
	Square p;
	while (to) {
		p = pop_lsb(&to);
		*list++ = Move(from, p, PR_KNIGHT);
		*list++ = Move(from, p, PR_BISHOP);
		*list++ = Move(from, p, PR_ROOK);
		*list++ = Move(from, p, PR_QUEEN);
	}
	return list;
}

template<>
inline Move* make<PROMOTION_CAPTURES>(Square from, Bitboard to, Move* list) {
	Square p;
	while (to) {
		p = pop_lsb(&to);
		*list++ = Move(from, p, PC_KNIGHT);
		*list++ = Move(from, p, PC_BISHOP);
		*list++ = Move(from, p, PC_ROOK);
		*list++ = Move(from, p, PC_QUEEN);
	}
	return list;
}

extern std::ostream& operator<<(std::ostream& os, const Move& m);

const Bitboard WHITE_OO_MASK = 0x90;
const Bitboard WHITE_OOO_MASK = 0x11;
const Bitboard WHITE_OO_BLOCKERS_AND_ATTACKERS_MASK = 0x60;
const Bitboard WHITE_OOO_BLOCKERS_AND_ATTACKERS_MASK = 0xe;
const Bitboard BLACK_OO_MASK = 0x9000000000000000;
const Bitboard BLACK_OOO_MASK = 0x1100000000000000;
const Bitboard BLACK_OO_BLOCKERS_AND_ATTACKERS_MASK = 0x6000000000000000;
const Bitboard BLACK_OOO_BLOCKERS_AND_ATTACKERS_MASK = 0xE00000000000000;
const Bitboard ALL_CASTLING_MASK = 0x9100000000000091;

template<Color C> constexpr Bitboard oo_mask() { return C == WHITE ? WHITE_OO_MASK : BLACK_OO_MASK; }
template<Color C> constexpr Bitboard ooo_mask() { return C == WHITE ? WHITE_OOO_MASK : BLACK_OOO_MASK; }

template<Color C>
constexpr Bitboard oo_blockers_mask() { 
	return C == WHITE ? WHITE_OO_BLOCKERS_AND_ATTACKERS_MASK :
		BLACK_OO_BLOCKERS_AND_ATTACKERS_MASK; 
}

template<Color C>
constexpr Bitboard ooo_blockers_mask() {
	return C == WHITE ? WHITE_OOO_BLOCKERS_AND_ATTACKERS_MASK :
		BLACK_OOO_BLOCKERS_AND_ATTACKERS_MASK;
}
	
template<Color C> constexpr Bitboard ignore_ooo_danger() { return C == WHITE ? 0x2 : 0x200000000000000; }
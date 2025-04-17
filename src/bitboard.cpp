#include "bitboard.h"
#include "lookup_tables.h"
#include <sstream>

uint64_t zobrist::zobrist_table[NPIECES][NSQUARES];

void zobrist::initialise_zobrist_keys() {
	PRNG rng(70026072);
	for (int i = 0; i < NPIECES; i++)
		for (int j = 0; j < NSQUARES; j++)
			zobrist::zobrist_table[i][j] = rng.rand<uint64_t>();
}

std::ostream& operator<< (std::ostream& os, const PositionManager& p) {
	os << "\n+---+---+---+---+---+---+---+---+\n";
	for (int rank = 7; rank >= 0; --rank) {
		os << "|";
		for (int file = 0; file < 8; ++file) {
			Piece piece = p.at(static_cast<Square>(rank * 8 + file));
			if (piece != NO_PIECE)
				os << " " << PIECE_STR[piece] << " |";
			else
				os << "   |";
		}
		os << " " << (rank + 1);
		os << "\n+---+---+---+---+---+---+---+---+\n";
	}
	os << "  a   b   c   d   e   f   g   h \n\n";

	os << "FEN: " << p.fen() << "\n";
	os << "Side to move: " << (p.turn() == WHITE ? "White" : "Black") << "\n";
	os << "EP: " << (p.history[p.ply()].epsq == NO_SQ ? " -" : SQUARE_STR[p.history[p.ply()].epsq]);
	return os;
}

std::string PositionManager::fen() const {
	std::ostringstream fen;
	int empty;

	for (int i = 56; i >= 0; i -= 8) {
		empty = 0;
		for (int j = 0; j < 8; j++) {
			Piece p = board[i + j];
			if (p == NO_PIECE) empty++;
			else {
				fen << (empty == 0 ? "" : std::to_string(empty))
					<< PIECE_STR[p];
				empty = 0;
			}
		}

		if (empty != 0) fen << empty;
		if (i > 0) fen << '/';
	}

	fen << (side_to_play == WHITE ? " w " : " b ")
		<< (history[game_ply].entry & WHITE_OO_MASK ? "" : "K")
		<< (history[game_ply].entry & WHITE_OOO_MASK ? "" : "Q")
		<< (history[game_ply].entry & BLACK_OO_MASK ? "" : "k")
		<< (history[game_ply].entry & BLACK_OOO_MASK ? "" : "q")
		<< (history[game_ply].entry & ALL_CASTLING_MASK ? "- " : "")
		<< (history[game_ply].epsq == NO_SQ ? " -" : SQUARE_STR[history[game_ply].epsq]);

	return fen.str();
}

void PositionManager::set(const std::string& fen, PositionManager& p) {
	int square = A8;
	for (char ch : fen.substr(0, fen.find(' '))) {
		if (isdigit(ch))
			square += (ch - '0') * EAST;
		else if (ch == '/')
			square += 2 * SOUTH;
		else
			p.put_piece(Piece(PIECE_STR.find(ch)), Square(square++));
	}

	std::istringstream ss(fen.substr(fen.find(' ')));
	unsigned char token;

	ss >> token;
	p.side_to_play = token == 'w' ? WHITE : BLACK;

	p.history[p.game_ply].entry = ALL_CASTLING_MASK;
	while (ss >> token && !isspace(token)) {
		switch (token) {
		case 'K':
			p.history[p.game_ply].entry &= ~WHITE_OO_MASK;
			break;
		case 'Q':
			p.history[p.game_ply].entry &= ~WHITE_OOO_MASK;
			break;
		case 'k':
			p.history[p.game_ply].entry &= ~BLACK_OO_MASK;
			break;
		case 'q':
			p.history[p.game_ply].entry &= ~BLACK_OOO_MASK;
			break;
		}
	}
}
	

void PositionManager::move_piece(Square from, Square to) {
	hash ^= zobrist::zobrist_table[board[from]][from] ^ zobrist::zobrist_table[board[from]][to]
		^ zobrist::zobrist_table[board[to]][to];
	Bitboard mask = SQUARE_BB[from] | SQUARE_BB[to];
	piece_bb[board[from]] ^= mask;
	piece_bb[board[to]] &= ~mask;
	board[to] = board[from];
	board[from] = NO_PIECE;
}

void PositionManager::move_piece_quiet(Square from, Square to) {
	hash ^= zobrist::zobrist_table[board[from]][from] ^ zobrist::zobrist_table[board[from]][to];
	piece_bb[board[from]] ^= (SQUARE_BB[from] | SQUARE_BB[to]);
	board[to] = board[from];
	board[from] = NO_PIECE;
}


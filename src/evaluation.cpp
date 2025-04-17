#include "evaluation.h"
#include "bitboard.h"
#include "chess_types.h"

const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 300;
const int BISHOP_VALUE = 300;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;

int eval(const PositionManager& position) {
  int whiteEval = count_material(position, WHITE);
  int blackEval = count_material(position, BLACK);

  int evaluation = whiteEval - blackEval;

  int perspective = position.turn() == WHITE ? 1 : -1;
  return evaluation * perspective;
}

int count_material(const PositionManager& position, const Color color) {
  int material = 0;

  material += sparse_pop_count(position.bitboard_of(color, PAWN)) * PAWN_VALUE;
  material += sparse_pop_count(position.bitboard_of(color, KNIGHT)) * KNIGHT_VALUE;
  material += sparse_pop_count(position.bitboard_of(color, BISHOP)) * BISHOP_VALUE;
  material += sparse_pop_count(position.bitboard_of(color, ROOK)) * ROOK_VALUE;
  material += sparse_pop_count(position.bitboard_of(color, QUEEN)) * QUEEN_VALUE;

  return material;
}
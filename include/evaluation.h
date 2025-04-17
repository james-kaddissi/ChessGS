#include "bitboard.h"
#include "chess_types.h"

int eval(const PositionManager& position);
int count_material(const PositionManager& position, const Color color);
#include "engine.h"
#include "pst.h"
#include <algorithm>
#include <cmath>

static constexpr int PAWN_VALUE = 100;
static constexpr int KNIGHT_VALUE = 300;
static constexpr int BISHOP_VALUE = 300;
static constexpr int ROOK_VALUE = 500;
static constexpr int QUEEN_VALUE = 900;
static constexpr int MG_TEMPO = 10;
static constexpr int EG_TEMPO = 5;
static constexpr int BISHOP_PAIR = 30;
static constexpr int P_KNIGHT_PAIR = 10;
static constexpr int P_ROOK_PAIR = 20;
static constexpr int PHASE_KNIGHT = 1;
static constexpr int PHASE_BISHOP = 1;
static constexpr int PHASE_ROOK = 2;
static constexpr int PHASE_QUEEN = 4;
static constexpr int PHASE_MAX = PHASE_KNIGHT * 4 + PHASE_BISHOP * 4 + PHASE_ROOK * 4 + PHASE_QUEEN * 2;

static inline int pst_index(Color c, Square sq) {
  return (c == WHITE) ? (sq ^ 56) : sq;
}

Bitboard ChessEngine::getFriendlyPieces(Color color) const {
  return (color == WHITE) ? position.all_pieces<WHITE>()
                          : position.all_pieces<BLACK>();
}

int ChessEngine::eval() {
  if (inEndgame()) {
    return evalEndgame();
  }

  int phase = game_phase();
  if (phase > PHASE_MAX)
    phase = PHASE_MAX;

  Score white = evaluate_color(WHITE);
  Score black = evaluate_color(BLACK);

  if (position.turn() == WHITE) {
    white.mg += MG_TEMPO;
    white.eg += EG_TEMPO;
  } else {
    black.mg += MG_TEMPO;
    black.eg += EG_TEMPO;
  }

  Score total = white - black;

  int mgScore = total.mg * phase;
  int egScore = total.eg * (PHASE_MAX - phase);
  int blended = (mgScore + egScore) / PHASE_MAX;

  int perspective = (position.turn() == WHITE) ? 1 : -1;
  return blended * perspective;
}

Score ChessEngine::evaluate_color(Color color) {
  Score eval;

  eval += count_material(color);
  eval += evalPawns(color);
  eval += evalKnights(color);
  eval += evalBishops(color);
  eval += evalRooks(color);
  eval += evalQueens(color);
  eval += evalKing(color);

  eval += evalPawnStructure(color);
  eval += evalKingVulnerability(color);

  return eval;
}

Score ChessEngine::evalPawnStructure(Color color) {
  Score score;
  Bitboard pawns = position.bitboard_of(color, PAWN);

  // doubled pawns penalty for each extra pawn on the same file
  for (int i = 0; i < 8; i++) {
    int count = sparse_pop_count(pawns & MASK_FILE[i]);
    if (count > 1) {
      score.mg -= 10 * (count - 1);
      score.eg -= 20 * (count - 1);
    }
  }

  // no friendly pawns on adjacent files
  Bitboard isolatedPawns = 0;
  for (int i = 0; i < 8; i++) {
    Bitboard file_pawns = pawns & MASK_FILE[i];
    if (file_pawns) {
      Bitboard adjacent_files = 0;
      if (i > 0)
        adjacent_files |= MASK_FILE[i - 1];
      if (i < 7)
        adjacent_files |= MASK_FILE[i + 1];
      if (!(pawns & adjacent_files))
        isolatedPawns |= file_pawns;
    }
  }
  score.mg -= 20 * sparse_pop_count(isolatedPawns);
  score.eg -= 10 * sparse_pop_count(isolatedPawns);

  // no enemy pawn on same or adjacent files in front of us
  Bitboard enemy_pawns = position.bitboard_of(~color, PAWN);
  Bitboard passed = 0;

  Bitboard work = pawns;
  while (work) {
    Square sq = pop_lsb(&work);
    int file = file_of(sq);

    Bitboard front_span_own_file = 0;
    if (color == WHITE) {
      for (int r = rank_of(sq) + 1; r <= RANK8; r++)
        front_span_own_file |= SQUARE_BB[create_square(File(file), Rank(r))];
    } else {
      for (int r = rank_of(sq) - 1; r >= RANK1; r--)
        front_span_own_file |= SQUARE_BB[create_square(File(file), Rank(r))];
    }

    Bitboard adjacent = 0;
    if (file > 0)
      adjacent |= MASK_FILE[file - 1];
    if (file < 7)
      adjacent |= MASK_FILE[file + 1];

    Bitboard adj_front = 0;
    if (color == WHITE) {
      for (int r = rank_of(sq) + 1; r <= RANK8; r++) {
        if (file > 0)
          adj_front |= SQUARE_BB[create_square(File(file - 1), Rank(r))];
        if (file < 7)
          adj_front |= SQUARE_BB[create_square(File(file + 1), Rank(r))];
      }
    } else {
      for (int r = rank_of(sq) - 1; r >= RANK1; r--) {
        if (file > 0)
          adj_front |= SQUARE_BB[create_square(File(file - 1), Rank(r))];
        if (file < 7)
          adj_front |= SQUARE_BB[create_square(File(file + 1), Rank(r))];
      }
    }

    Bitboard sentinel_zone = front_span_own_file | adj_front;
    if (!(sentinel_zone & enemy_pawns)) {
      passed |= SQUARE_BB[sq];
    }
  }

  while (passed) {
    Square sq = pop_lsb(&passed);
    int rank = rank_of(sq);
    int color_based_rank = (color == WHITE) ? rank : 7 - rank;

    score.mg += 10 * (color_based_rank + 1) * (color_based_rank + 1);
    score.eg += 20 * (color_based_rank + 1) * (color_based_rank + 1);
  }

  return score;
}

Score ChessEngine::evalKingVulnerability(Color color) {
  Score score;
  Square kingSquare = bsf(position.bitboard_of(color, KING));

  // pawn protection around the king
  Bitboard kingSpace = KING_ATTACKS[kingSquare] | SQUARE_BB[kingSquare];
  Bitboard friendlyPawns = position.bitboard_of(color, PAWN);
  int protection = sparse_pop_count(kingSpace & friendlyPawns);
  score.mg += 10 * protection;

  Color enemy = ~color;
  Bitboard enemyKnights = position.bitboard_of(enemy, KNIGHT);
  Bitboard enemyBishops = position.bitboard_of(enemy, BISHOP);
  Bitboard enemyRooks = position.bitboard_of(enemy, ROOK);
  Bitboard enemyQueens = position.bitboard_of(enemy, QUEEN);

  int knightThreats = 0, bishopThreats = 0, rookThreats = 0, queenThreats = 0;
  Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();

  while (enemyKnights) {
    Square sq = pop_lsb(&enemyKnights);
    if (KNIGHT_ATTACKS[sq] & kingSpace)
      knightThreats++;
  }
  while (enemyBishops) {
    Square sq = pop_lsb(&enemyBishops);
    if (get_bishop_attacks(sq, occ) & kingSpace)
      bishopThreats++;
  }
  while (enemyRooks) {
    Square sq = pop_lsb(&enemyRooks);
    if (get_rook_attacks(sq, occ) & kingSpace)
      rookThreats++;
  }
  while (enemyQueens) {
    Square sq = pop_lsb(&enemyQueens);
    if ((get_bishop_attacks(sq, occ) | get_rook_attacks(sq, occ)) & kingSpace)
      queenThreats++;
  }

  int threatScore = knightThreats * 20 + bishopThreats * 20 + rookThreats * 40 + queenThreats * 80;
  if (threatScore > 0) {
    score.mg -= threatScore * threatScore / 50;
  }
  return score;
}

int ChessEngine::count_material(Color color) {
  int material = 0;
  material += sparse_pop_count(position.bitboard_of(color, PAWN)) * PAWN_VALUE;
  material +=
      sparse_pop_count(position.bitboard_of(color, KNIGHT)) * KNIGHT_VALUE;
  material +=
      sparse_pop_count(position.bitboard_of(color, BISHOP)) * BISHOP_VALUE;
  material += sparse_pop_count(position.bitboard_of(color, ROOK)) * ROOK_VALUE;
  material +=
      sparse_pop_count(position.bitboard_of(color, QUEEN)) * QUEEN_VALUE;
  return material;
}

int ChessEngine::game_phase() {
  int phase = 0;
  phase += sparse_pop_count(position.bitboard_of(WHITE, KNIGHT) |
                            position.bitboard_of(BLACK, KNIGHT)) *
           PHASE_KNIGHT;
  phase += sparse_pop_count(position.bitboard_of(WHITE, BISHOP) |
                            position.bitboard_of(BLACK, BISHOP)) *
           PHASE_BISHOP;
  phase += sparse_pop_count(position.bitboard_of(WHITE, ROOK) |
                            position.bitboard_of(BLACK, ROOK)) *
           PHASE_ROOK;
  phase += sparse_pop_count(position.bitboard_of(WHITE, QUEEN) |
                            position.bitboard_of(BLACK, QUEEN)) *
           PHASE_QUEEN;
  return phase;
}

Score ChessEngine::evalPawns(Color color) {
  Score score;
  Bitboard pawns = position.bitboard_of(color, PAWN);
  while (pawns) {
    Square sq = pop_lsb(&pawns);
    int idx = pst_index(color, sq);
    score.mg += MG_PAWN_PST[idx];
    score.eg += EG_PAWN_PST[idx];
  }
  return score;
}

Score ChessEngine::evalKnightMobility(Square sq, Color color, Bitboard poss) {
  (void)sq;
  (void)color;
  Score score;
  int mobility = sparse_pop_count(poss);
  score.mg += 4 * (mobility - 4);
  score.eg += 6 * (mobility - 4);
  return score;
}

Score ChessEngine::evalKnights(Color color) {
  Score score;
  Bitboard knights = position.bitboard_of(color, KNIGHT);

  // pair penalty
  if (sparse_pop_count(knights) > 1) {
    score.mg -= P_KNIGHT_PAIR;
    score.eg -= P_KNIGHT_PAIR;
  }

  int attack = 0;
  Square enemyKingSq = bsf(position.bitboard_of(~color, KING));
  Bitboard enemyKingZone = KING_ATTACKS[enemyKingSq];

  while (knights) {
    Square sq = pop_lsb(&knights);
    int idx = pst_index(color, sq);
    score.mg += MG_KNIGHT_PST[idx];
    score.eg += EG_KNIGHT_PST[idx];

    Bitboard kn_attacks = KNIGHT_ATTACKS[sq];
    Bitboard reachable = kn_attacks & ~getFriendlyPieces(color);
    score += evalKnightMobility(sq, color, reachable);

    // king attack bonus
    attack += sparse_pop_count(kn_attacks & enemyKingZone);
  }

  score.mg += 2 * attack;
  score.eg += 2 * attack;
  return score;
}

Score ChessEngine::evalBishops(Color color) {
  Score score;
  Bitboard bishops = position.bitboard_of(color, BISHOP);

  if (sparse_pop_count(bishops) > 1) {
    score.mg += BISHOP_PAIR;
    score.eg += BISHOP_PAIR;
  }

  int totalMobility = 0;
  int attack = 0;
  Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
  Square enemyKingSq = bsf(position.bitboard_of(~color, KING));
  Bitboard enemyKingZone = KING_ATTACKS[enemyKingSq];

  while (bishops) {
    Square sq = pop_lsb(&bishops);
    int idx = pst_index(color, sq);
    score.mg += MG_BISHOP_PST[idx];
    score.eg += EG_BISHOP_PST[idx];

    Bitboard b_attacks = get_bishop_attacks(sq, occ);
    Bitboard reachable = b_attacks & ~getFriendlyPieces(color);
    totalMobility += sparse_pop_count(reachable);

    attack += sparse_pop_count(b_attacks & enemyKingZone);
  }

  score.mg += 3 * (totalMobility - 7);
  score.eg += 3 * (totalMobility - 7);
  score.mg += 2 * attack;
  score.eg += 2 * attack;
  return score;
}

Score ChessEngine::evalRooks(Color color) {
  Score score;
  Bitboard rooks = position.bitboard_of(color, ROOK);

  if (sparse_pop_count(rooks) > 1) {
    score.mg -= P_ROOK_PAIR;
    score.eg -= P_ROOK_PAIR;
  }

  int totalMobility = 0;
  int attack = 0;
  Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
  Square enemyKingSq = bsf(position.bitboard_of(~color, KING));
  Bitboard enemyKingZone = KING_ATTACKS[enemyKingSq];

  while (rooks) {
    Square sq = pop_lsb(&rooks);
    int idx = pst_index(color, sq);
    score.mg += MG_ROOK_PST[idx];
    score.eg += EG_ROOK_PST[idx];

    Bitboard r_attacks = get_rook_attacks(sq, occ);
    Bitboard reachable = r_attacks & ~getFriendlyPieces(color);
    totalMobility += sparse_pop_count(reachable);

    attack += sparse_pop_count(r_attacks & enemyKingZone);
  }

  score.mg += 2 * (totalMobility - 7);
  score.eg += 4 * (totalMobility - 7);
  score.mg += 3 * attack;
  score.eg += 3 * attack;
  return score;
}

Score ChessEngine::evalQueens(Color color) {
  Score score;
  Bitboard queens = position.bitboard_of(color, QUEEN);

  int totalMobility = 0;
  int attack = 0;
  Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
  Square enemyKingSq = bsf(position.bitboard_of(~color, KING));
  Bitboard enemyKingZone = KING_ATTACKS[enemyKingSq];

  while (queens) {
    Square sq = pop_lsb(&queens);
    int idx = pst_index(color, sq);
    score.mg += MG_QUEEN_PST[idx];
    score.eg += EG_QUEEN_PST[idx];

    // early development penalty for queen if king is still on back rank and hasn't moved
    if (color == WHITE && rank_of(sq) > RANK2) {
      if (position.at(B1) == WHITE_KNIGHT)
        score.mg -= 2;
      if (position.at(C1) == WHITE_BISHOP)
        score.mg -= 2;
      if (position.at(F1) == WHITE_BISHOP)
        score.mg -= 2;
      if (position.at(G1) == WHITE_KNIGHT)
        score.mg -= 2;
    } else if (color == BLACK && rank_of(sq) < RANK7) {
      if (position.at(B8) == BLACK_KNIGHT)
        score.mg -= 2;
      if (position.at(C8) == BLACK_BISHOP)
        score.mg -= 2;
      if (position.at(F8) == BLACK_BISHOP)
        score.mg -= 2;
      if (position.at(G8) == BLACK_KNIGHT)
        score.mg -= 2;
    }

    Bitboard q_attacks =
        get_rook_attacks(sq, occ) | get_bishop_attacks(sq, occ);
    Bitboard reachable = q_attacks & ~getFriendlyPieces(color);
    totalMobility += sparse_pop_count(reachable);

    attack += sparse_pop_count(q_attacks & enemyKingZone);
  }

  // mobility bonus.
  score.mg += 1 * (totalMobility - 14);
  score.eg += 2 * (totalMobility - 14);
  score.mg += 4 * attack;
  score.eg += 4 * attack;
  return score;
}

Score ChessEngine::evalKing(Color color) {
  Score score;
  Bitboard king = position.bitboard_of(color, KING);
  if (!king)
    return score;
  Square sq = bsf(king);
  int idx = pst_index(color, sq);
  score.mg += MG_KING_PST[idx];
  score.eg += EG_KING_PST[idx];
  return score;
}

bool ChessEngine::inEndgame() {
  bool noQueens = (position.bitboard_of(WHITE, QUEEN) |
                   position.bitboard_of(BLACK, QUEEN)) == 0;

  int totalNonPawnMaterial =
      sparse_pop_count(position.bitboard_of(WHITE, KNIGHT)) * KNIGHT_VALUE +
      sparse_pop_count(position.bitboard_of(WHITE, BISHOP)) * BISHOP_VALUE +
      sparse_pop_count(position.bitboard_of(WHITE, ROOK)) * ROOK_VALUE +
      sparse_pop_count(position.bitboard_of(WHITE, QUEEN)) * QUEEN_VALUE +
      sparse_pop_count(position.bitboard_of(BLACK, KNIGHT)) * KNIGHT_VALUE +
      sparse_pop_count(position.bitboard_of(BLACK, BISHOP)) * BISHOP_VALUE +
      sparse_pop_count(position.bitboard_of(BLACK, ROOK)) * ROOK_VALUE +
      sparse_pop_count(position.bitboard_of(BLACK, QUEEN)) * QUEEN_VALUE;

  return noQueens || totalNonPawnMaterial < 1500;
}

int ChessEngine::evalEndgame() {
  Score score;
  score += evaluate_color(WHITE) - evaluate_color(BLACK);

  Square whiteKing = bsf(position.bitboard_of(WHITE, KING));
  Square blackKing = bsf(position.bitboard_of(BLACK, KING));

  int wkf = file_of(whiteKing);
  int wkr = rank_of(whiteKing);
  int bkf = file_of(blackKing);
  int bkr = rank_of(blackKing);
  int wfd = std::max(3 - wkf, wkf - 4);
  int bfd = std::max(3 - bkf, bkf - 4);
  int wrd = std::max(3 - wkr, wkr - 4);
  int brd = std::max(3 - bkr, bkr - 4);

  int wcd = wfd + wrd;
  int bcd = bfd + brd;

  score.eg += (bcd - wcd) * 10;

  int kingDistance = std::max(std::abs(wkf - bkf), std::abs(wkr - bkr));

  if (sparse_pop_count(position.bitboard_of(WHITE, PAWN) |
                       position.bitboard_of(BLACK, PAWN)) == 0) {
    bool pat = (kingDistance % 2 == 0) && (getSideToMove() == BLACK);
    if (pat) {
      score.eg += 20;
    }
  }

  int perspective = (position.turn() == WHITE) ? 1 : -1;
  return score.eg * perspective;
}
#include "engine.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>

static constexpr int MATE_SCORE = 100000;
static constexpr int INF = 1000000;
static constexpr int MATE_BOUND = MATE_SCORE - MAX_PLY;

static constexpr int ASP_INITIAL_WINDOW = 50;
static constexpr int ASP_MIN_DEPTH = 4;

static constexpr int SEE_PIECE_VALUE[7] = {100, 300, 300, 500, 900, 20000, 0};

static int safe_pct(uint64_t num, uint64_t den) {
  return den == 0 ? 0 : (int)((num * 100) / den);
}

int ChessEngine::scoreToTT(int score, int ply) {
  if (score >= MATE_BOUND)
    return score + ply;
  if (score <= -MATE_BOUND)
    return score - ply;
  return score;
}
int ChessEngine::scoreFromTT(int score, int ply) {
  if (score >= MATE_BOUND)
    return score - ply;
  if (score <= -MATE_BOUND)
    return score + ply;
  return score;
}

void ChessEngine::initLmrTable() {
  for (int d = 0; d < 64; d++) {
    for (int m = 0; m < 64; m++) {
      if (d < 3 || m < 3) {
        lmr_reductions[d][m] = 0;
      } else {
        double r = 0.5 + std::log((double)d) * std::log((double)m) / 2.25;
        int rint = (int)r;
        if (rint < 0)
          rint = 0;
        lmr_reductions[d][m] = rint;
      }
    }
  }
}

void ChessEngine::ttStore(uint64_t key, int depth, int score, TTBound bound,
                          Move bestMove, int ply) {
  TTEntry &e = tt[key & TT_MASK];

  bool replace;
  if (e.key == 0) {
    replace = true;
  } else if (e.age != tt_age) {
    replace = true;
  } else if (e.key == key) {
    if (e.depth > depth && e.bound == TT_EXACT && bound != TT_EXACT)
      return;
    replace = true;
  } else {
    replace = depth >= e.depth - 2;
  }

  if (!replace)
    return;

  e.key = key;
  e.depth = static_cast<int8_t>(std::max(-128, std::min(127, depth)));
  e.score = static_cast<int16_t>(scoreToTT(score, ply));
  e.bound = static_cast<uint8_t>(bound);
  e.bestMove = bestMove;
  e.age = tt_age;
}

bool ChessEngine::ttProbe(uint64_t key, int depth, int alpha, int beta, int ply,
                          int &score, Move &bestMove) {
  const TTEntry &e = tt[key & TT_MASK];
  if (e.key != key)
    return false;

  bestMove = e.bestMove;
  if (e.depth < depth)
    return false;

  int s = scoreFromTT(e.score, ply);
  switch (e.bound) {
  case TT_EXACT:
    score = s;
    return true;
  case TT_LOWER:
    if (s >= beta) {
      score = s;
      return true;
    }
    return false;
  case TT_UPPER:
    if (s <= alpha) {
      score = s;
      return true;
    }
    return false;
  }
  return false;
}

int ChessEngine::getCaptureScore(const Move &move) {
  if (!move.is_capture())
    return 0;

  Color capcolor;
  PieceType captype = getPieceAt(move.to(), capcolor);
  Color attackercolor;
  PieceType attackertype = getPieceAt(move.from(), attackercolor);

  if (move.flags() == EN_PASSANT) {
    captype = PAWN;
  }

  static const int vs[7] = {100, 300, 300, 500, 900, 0, 0};
  static const int as[7] = {1, 3, 3, 5, 9, 0, 0};

  if (captype == NONE)
    captype = PAWN;

  return vs[captype] * 10 - as[attackertype];
}

int ChessEngine::see(const Move &move) {
  Square from = move.from();
  Square to = move.to();
  MoveFlags flags = move.flags();

  int gain[32];
  int d = 0;

  Color side = position.turn();
  Color attackerColor = side;

  Color tmpC;
  PieceType movingPiece = getPieceAt(from, tmpC);

  PieceType capturedPiece;
  Square epCapturedSquare = NO_SQ;
  if (flags == EN_PASSANT) {
    capturedPiece = PAWN;
    epCapturedSquare = (side == WHITE) ? Square(to - 8) : Square(to + 8);
  } else if (!move.is_capture()) {
    return 0;
  } else {
    capturedPiece = getPieceAt(to, tmpC);
    if (capturedPiece == NONE)
      return 0;
  }

  bool promoCapture = (flags >= PC_KNIGHT && flags <= PC_QUEEN);
  int promoBonus = 0;
  if (promoCapture) {
    promoBonus = SEE_PIECE_VALUE[QUEEN] - SEE_PIECE_VALUE[PAWN];
    movingPiece = QUEEN;
  }

  gain[d] = SEE_PIECE_VALUE[capturedPiece] + promoBonus;

  Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
  occ &= ~SQUARE_BB[from];
  if (epCapturedSquare != NO_SQ)
    occ &= ~SQUARE_BB[epCapturedSquare];
  PieceType currentAttacker = movingPiece;
  attackerColor = ~side;

  while (true) {
    Bitboard attackers;
    if (attackerColor == WHITE)
      attackers = position.attackers_from<WHITE>(to, occ);
    else
      attackers = position.attackers_from<BLACK>(to, occ);
    attackers |= KING_ATTACKS[to] & position.bitboard_of(attackerColor, KING);
    attackers &= occ;

    if (!attackers)
      break;

    static const PieceType ORDER[6] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};
    PieceType lva = NONE;
    Square lvaSq = NO_SQ;
    for (int j = 0; j < 6; j++) {
      Bitboard candidates =
          attackers & position.bitboard_of(attackerColor, ORDER[j]);
      if (candidates) {
        lva = ORDER[j];
        lvaSq = bsf(candidates);
        break;
      }
    }
    if (lva == NONE)
      break;

    d++;
    gain[d] = SEE_PIECE_VALUE[currentAttacker] - gain[d - 1];

    if (std::max(-gain[d - 1], gain[d]) < 0)
      break;

    occ &= ~SQUARE_BB[lvaSq];
    currentAttacker = lva;
    attackerColor = ~attackerColor;

    if (lva == KING)
      break;

    if (d >= 31)
      break;
  }

  while (d > 0) {
    gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    d--;
  }
  return gain[0];
}

void ChessEngine::orderMovesInto(const Move *moves, int n, int ply, Move ttMove,
                                 ScoredMove *out) {
  Color side = position.turn();

  for (int i = 0; i < n; i++) {
    const Move &move = moves[i];
    int score = 0;

    if (ttMove != Move() && move == ttMove) {
      score = 1'000'000;
    } else if (move.flags() == PR_QUEEN || move.flags() == PC_QUEEN) {
      score = 250'000;
    } else if (move.is_capture()) {
      score = 200'000 + getCaptureScore(move);
    } else if (move.flags() >= PR_KNIGHT && move.flags() <= PR_ROOK) {
      score = 110'000 + (move.flags() - PR_KNIGHT) * 100;
    } else if (ply < MAX_PLY && move == killer_moves[ply][0]) {
      score = 100'000;
    } else if (ply < MAX_PLY && move == killer_moves[ply][1]) {
      score = 90'000;
    } else {
      score = history_table[side][move.from()][move.to()];
    }
    out[i] = {move, score};
  }

  std::sort(out, out + n, [](const ScoredMove &a, const ScoredMove &b) {
    return a.score > b.score;
  });
}

int ChessEngine::generateLegalMovesInto(Move *buf) {
  if (position.turn() == WHITE) {
    Move *end = position.generate_legals<WHITE>(buf);
    return (int)(end - buf);
  } else {
    Move *end = position.generate_legals<BLACK>(buf);
    return (int)(end - buf);
  }
}

void ChessEngine::clearTables() {
  std::memset(history_table, 0, sizeof(history_table));
  clearKillers();
  std::memset(tt.data(), 0, tt.size() * sizeof(TTEntry));
  tt_age = 0;
}

void ChessEngine::clearKillers() {
  for (int i = 0; i < MAX_PLY; i++) {
    killer_moves[i][0] = Move();
    killer_moves[i][1] = Move();
  }
}

void ChessEngine::updateKillerMoves(const Move &move, int ply) {
  if (ply < 0 || ply >= MAX_PLY)
    return;
  if (move != killer_moves[ply][0]) {
    killer_moves[ply][1] = killer_moves[ply][0];
    killer_moves[ply][0] = move;
  }
}

void ChessEngine::updateHistoryTable(const Move &move, int depth, Color side) {
  history_table[side][move.from()][move.to()] += depth * depth;
  if (history_table[side][move.from()][move.to()] > 1'000'000) {
    for (int s = 0; s < 2; s++)
      for (int f = 0; f < 64; f++)
        for (int t = 0; t < 64; t++)
          history_table[s][f][t] /= 2;
  }
}

bool ChessEngine::isRepetition() const {
  if (repetition_history.size() < 2)
    return false;
  uint64_t cur = position.get_hash();
  for (size_t i = repetition_history.size() - 1; i > 0;) {
    --i;
    if (repetition_history[i] == cur)
      return true;
  }
  return false;
}

bool ChessEngine::isDrawByInsufficientMaterial() const {
  int wp = sparse_pop_count(position.bitboard_of(WHITE, PAWN));
  int bp = sparse_pop_count(position.bitboard_of(BLACK, PAWN));
  int wq = sparse_pop_count(position.bitboard_of(WHITE, QUEEN));
  int bq = sparse_pop_count(position.bitboard_of(BLACK, QUEEN));
  int wr = sparse_pop_count(position.bitboard_of(WHITE, ROOK));
  int br = sparse_pop_count(position.bitboard_of(BLACK, ROOK));
  int wn = sparse_pop_count(position.bitboard_of(WHITE, KNIGHT));
  int bn = sparse_pop_count(position.bitboard_of(BLACK, KNIGHT));
  int wb = sparse_pop_count(position.bitboard_of(WHITE, BISHOP));
  int bb = sparse_pop_count(position.bitboard_of(BLACK, BISHOP));

  if (wp || bp || wq || bq || wr || br)
    return false;
  int wMinors = wn + wb;
  int bMinors = bn + bb;
  if (wMinors == 0 && bMinors == 0)
    return true;
  if (wMinors == 1 && bMinors == 0)
    return true;
  if (wMinors == 0 && bMinors == 1)
    return true;
  return false;
}

bool ChessEngine::checkTimeUp() {
  if (allocated_time_ms == 0)
    return false;
  if ((searchStats.nodes & (nodes_between_checks - 1)) == 0) {
    if ((Sint32)(SDL_GetTicks() - start_time) >
        (Sint32)(allocated_time_ms * 0.8)) {
      time_up_flag = true;
      return true;
    }
  }
  return time_up_flag;
}

int ChessEngine::search(int depth, int ply, int alpha, int beta, bool nullPrune,
                        bool isPv) {
  if (checkTimeUp())
    return alpha;
  searchStats.nodes++;
  search_progress.nodes.store(searchStats.nodes, std::memory_order_relaxed);

  if (ply > 0 && (isRepetition() || isDrawByInsufficientMaterial())) {
    return 0;
  }

  if (ply > 0) {
    int mating = MATE_SCORE - ply;
    int mated = -MATE_SCORE + ply;
    if (alpha < mated)
      alpha = mated;
    if (beta > mating)
      beta = mating;
    if (alpha >= beta)
      return alpha;
  }

  uint64_t hash = position.get_hash();
  Move ttMove;
  int ttScore;
  if (!isPv && ttProbe(hash, depth, alpha, beta, ply, ttScore, ttMove)) {
    searchStats.hash_used++;
    searchStats.hash_hits++;
    return ttScore;
  } else {
    const TTEntry &e = tt[hash & TT_MASK];
    if (e.key == hash)
      ttMove = e.bestMove;
  }

  if (depth <= 0) {
    return quiescence_search(alpha, beta);
  }

  bool inCheck = isInCheck(getSideToMove());

  if (nullPrune && !isPv && depth >= 3 && !inCheck &&
      non_pawn_material(getSideToMove()) > 0) {
    Square savedEp = position.history[position.game_ply].epsq;
    Bitboard savedEntry = position.history[position.game_ply].entry;

    position.flip_side_hash();
    position.xor_ep_hash(savedEp);
    position.side_to_play = ~position.side_to_play;
    position.game_ply++;
    position.history[position.game_ply] = UndoInfo();
    position.history[position.game_ply].entry = savedEntry;
    position.history[position.game_ply].epsq = NO_SQ;

    repetition_history.push_back(position.get_hash());

    int R = 3;
    int score = -search(depth - 1 - R, ply + 1, -beta, -beta + 1, false, false);

    repetition_history.pop_back();
    position.game_ply--;
    position.side_to_play = ~position.side_to_play;
    position.flip_side_hash();
    position.xor_ep_hash(savedEp);

    if (time_up_flag)
      return alpha;

    if (score >= beta) {
      searchStats.null_prunes++;
      return beta;
    }
  }

  Move moves[MAX_MOVES];
  int n = generateLegalMovesInto(moves);

  if (n == 0) {
    if (inCheck)
      return -MATE_SCORE + ply;
    return 0;
  }

  ScoredMove scored[MAX_MOVES];
  orderMovesInto(moves, n, ply, ttMove, scored);

  int old_alpha = alpha;
  Move currentBestMove;
  int bestScore = -INF;

  for (int i = 0; i < n; i++) {
    const Move &m = scored[i].move;
    bool isCapture = m.is_capture();
    bool isPromotion = (m.flags() >= PR_KNIGHT && m.flags() <= PR_QUEEN) ||
                       (m.flags() >= PC_KNIGHT && m.flags() <= PC_QUEEN);

    makeMove(m);
    bool givesCheck = isInCheck(getSideToMove());
    searchStats.moves_searched++;

    int evaluation;
    int newDepth = depth - 1;

    if (i == 0) {
      evaluation = -search(newDepth, ply + 1, -beta, -alpha, true, isPv);
    } else {
      int reduction = 0;
      if (depth >= 3 && i >= 3 && !isCapture && !isPromotion && !inCheck &&
          !givesCheck) {
        int dIdx = std::min(depth, 63);
        int mIdx = std::min(i, 63);
        reduction = lmr_reductions[dIdx][mIdx];
        if (!isPv && reduction > 0)
          reduction++;
        if (reduction >= newDepth)
          reduction = newDepth - 1;
        if (reduction < 0)
          reduction = 0;
      }

      evaluation = -search(newDepth - reduction, ply + 1, -alpha - 1, -alpha, true, false);
      if (reduction > 0 && evaluation > alpha) {
        evaluation =
            -search(newDepth, ply + 1, -alpha - 1, -alpha, true, false);
      }

      if (evaluation > alpha && evaluation < beta) {
        evaluation = -search(newDepth, ply + 1, -beta, -alpha, true, true);
      }
    }

    unmakeMove();

    if (time_up_flag)
      return alpha;

    if (evaluation > bestScore) {
      bestScore = evaluation;
      currentBestMove = m;
    }

    if (evaluation >= beta) {
      searchStats.fail_high++;
      if (i == 0)
        searchStats.fail_high_first++;
      if (!isCapture) {
        updateKillerMoves(m, ply);
        updateHistoryTable(m, depth, position.turn());
      }
      ttStore(hash, depth, evaluation, TT_LOWER, m, ply);
      return evaluation;
    }

    if (evaluation > alpha) {
      alpha = evaluation;
    }
  }

  TTBound bound = (alpha > old_alpha) ? TT_EXACT : TT_UPPER;
  ttStore(hash, depth, bestScore, bound, currentBestMove, ply);
  return bestScore;
}

int ChessEngine::quiescence_search(int alpha, int beta, int qdepth) {
  if (qdepth >= MAX_Q_DEPTH || checkTimeUp())
    return eval();
  searchStats.nodes++;
  searchStats.qnodes++;
  search_progress.nodes.store(searchStats.nodes, std::memory_order_relaxed);
  search_progress.qnodes.store(searchStats.qnodes, std::memory_order_relaxed);

  int stand_pat = eval();

  if (stand_pat >= beta)
    return stand_pat;
  if (stand_pat > alpha)
    alpha = stand_pat;

  Move moves[MAX_MOVES];
  int n = generateLegalMovesInto(moves);

  Move captures[MAX_MOVES];
  int cn = 0;
  for (int i = 0; i < n; i++) {
    const Move &m = moves[i];
    if (m.is_capture())
      captures[cn++] = m;
    else if (m.flags() == PR_QUEEN)
      captures[cn++] = m;
  }

  std::sort(captures, captures + cn, [this](const Move &a, const Move &b) {
    return getCaptureScore(a) > getCaptureScore(b);
  });

  constexpr int DELTA_MARGIN = 200;
  static const int piece_val[7] = {100, 300, 300, 500, 900, 0, 0};

  for (int i = 0; i < cn; i++) {
    const Move &move = captures[i];

    int capValue = 0;
    if (move.flags() == EN_PASSANT) {
      capValue = piece_val[PAWN];
    } else if (move.is_capture()) {
      Color cc;
      PieceType ct = getPieceAt(move.to(), cc);
      if (ct < 6)
        capValue = piece_val[ct];
    }
    bool is_promo_capture =
        move.flags() >= PC_KNIGHT && move.flags() <= PC_QUEEN;
    bool is_promo_quiet =
        move.flags() >= PR_KNIGHT && move.flags() <= PR_QUEEN;
    if (is_promo_capture || is_promo_quiet) {
      capValue += piece_val[QUEEN] - piece_val[PAWN];
    }

    if (stand_pat + capValue + DELTA_MARGIN < alpha)
      continue;

    if (!is_promo_capture && move.flags() != EN_PASSANT) {
      if (see(move) < 0)
        continue;
    }

    makeMove(move);
    int evaluation = -quiescence_search(-beta, -alpha, qdepth + 1);
    unmakeMove();

    if (time_up_flag)
      return alpha;
    if (evaluation >= beta)
      return evaluation;
    if (evaluation > alpha)
      alpha = evaluation;
  }
  return alpha;
}

Move ChessEngine::getBestMove(int maxDepth) {
  Move bookMove = getOpeningBookMove();
  if (bookMove != Move())
    return bookMove;

  {
    std::lock_guard<std::mutex> lk(iteration_log_mutex);
    iteration_log.clear();
  }

  start_time = SDL_GetTicks();
  allocated_time_ms = 0;
  time_up_flag = false;

  clearKillers();
  tt_age = (uint8_t)(tt_age + 1);
  if (tt_age == 0)
    tt_age = 1; 

  search_progress.active.store(true, std::memory_order_relaxed);
  search_progress.start_ms.store(start_time, std::memory_order_relaxed);
  search_progress.depth.store(0, std::memory_order_relaxed);
  search_progress.completed_depth.store(0, std::memory_order_relaxed);
  search_progress.nodes.store(0, std::memory_order_relaxed);
  search_progress.qnodes.store(0, std::memory_order_relaxed);
  search_progress.hash_hits.store(0, std::memory_order_relaxed);
  search_progress.fail_high.store(0, std::memory_order_relaxed);
  search_progress.fail_high_first.store(0, std::memory_order_relaxed);

  Move bestMove;
  int bestScore = -INF;

  for (int depth = 1; depth <= maxDepth; depth++) {
    search_progress.depth.store(depth, std::memory_order_relaxed);

    int alpha, beta;
    int window;
    if (depth >= ASP_MIN_DEPTH && std::abs(bestScore) < MATE_BOUND) {
      window = ASP_INITIAL_WINDOW;
      alpha = bestScore - window;
      beta = bestScore + window;
    } else {
      window = INF;
      alpha = -INF;
      beta = INF;
    }

    Move iterationBestMove;
    int iterationBestScore = -INF;
    bool iterationOk = false;

    while (true) {
      Move moves[MAX_MOVES];
      int n = generateLegalMovesInto(moves);
      if (n == 0)
        return Move();

      if (bestMove != Move()) {
        for (int i = 0; i < n; i++) {
          if (moves[i] == bestMove) {
            std::swap(moves[0], moves[i]);
            break;
          }
        }
      }

      iterationBestMove = Move();
      iterationBestScore = -INF;
      int rootAlpha = alpha;

      bool first = true;
      for (int i = 0; i < n; i++) {
        const Move &move = moves[i];
        makeMove(move);
        int evaluation;
        if (first) {
          evaluation =
              -search(depth - 1, 1, -beta, -rootAlpha, true, true);
        } else {
          evaluation =
              -search(depth - 1, 1, -rootAlpha - 1, -rootAlpha, true, false);
          if (evaluation > rootAlpha && evaluation < beta) {
            evaluation =
                -search(depth - 1, 1, -beta, -rootAlpha, true, true);
          }
        }
        unmakeMove();

        if (evaluation > iterationBestScore) {
          iterationBestScore = evaluation;
          iterationBestMove = move;
          if (evaluation > rootAlpha)
            rootAlpha = evaluation;
        }
        first = false;
      }

      if (iterationBestScore <= alpha) {
        window *= 2;
        alpha = std::max(-INF, iterationBestScore - window);
        if (window > 4 * ASP_INITIAL_WINDOW) {
          alpha = -INF;
          beta = INF;
        }
        continue;
      }
      if (iterationBestScore >= beta) {
        window *= 2;
        beta = std::min(INF, iterationBestScore + window);
        if (window > 4 * ASP_INITIAL_WINDOW) {
          alpha = -INF;
          beta = INF;
        }
        continue;
      }
      iterationOk = true;
      break;
    }

    if (!iterationOk)
      break;

    bestMove = iterationBestMove;
    bestScore = iterationBestScore;
    last_search_depth = depth;
    last_score = bestScore;

    search_progress.completed_depth.store(depth, std::memory_order_relaxed);
    search_progress.score_cp.store(bestScore, std::memory_order_relaxed);
    search_progress.hash_hits.store(searchStats.hash_hits,
                                    std::memory_order_relaxed);
    search_progress.fail_high.store(searchStats.fail_high,
                                    std::memory_order_relaxed);
    search_progress.fail_high_first.store(searchStats.fail_high_first,
                                          std::memory_order_relaxed);

    {
      IterationInfo info;
      info.depth = depth;
      info.score_cp = bestScore;
      info.nodes = (uint64_t)searchStats.nodes;
      info.time_ms = SDL_GetTicks() - start_time;
      info.pv = moveToUCI(bestMove);
      info.hash_hit_pct = safe_pct(searchStats.hash_hits, searchStats.nodes);
      info.fail_high_first_pct =
          safe_pct(searchStats.fail_high_first, searchStats.fail_high);
      info.effective_branching_x100 =
          searchStats.nodes == 0
              ? 0
              : (int)((uint64_t)searchStats.moves_searched * 100 /
                      searchStats.nodes);
      std::lock_guard<std::mutex> lk(iteration_log_mutex);
      iteration_log.push_back(std::move(info));
    }

    if (std::abs(bestScore) > MATE_BOUND)
      break;
  }
  search_progress.active.store(false, std::memory_order_relaxed);

  return bestMove;
}

Move ChessEngine::getBestMoveWithTime(int time_ms) {
  Move bookMove = getOpeningBookMove();
  if (bookMove != Move())
    return bookMove;

  {
    std::lock_guard<std::mutex> lk(iteration_log_mutex);
    iteration_log.clear();
  }

  start_time = SDL_GetTicks();
  allocated_time_ms = time_ms;
  time_up_flag = false;

  clearKillers();
  tt_age = (uint8_t)(tt_age + 1);
  if (tt_age == 0)
    tt_age = 1;

  Move bestMove;
  int bestScore = -INF;

  for (int depth = 1; depth <= MAX_B_DEPTH; depth++) {
    if (time_up_flag)
      break;

    Uint32 elapsed = SDL_GetTicks() - start_time;
    if (depth > 1 && elapsed > (Uint32)(time_ms / 2))
      break;

    int alpha, beta;
    int window;
    if (depth >= ASP_MIN_DEPTH && std::abs(bestScore) < MATE_BOUND) {
      window = ASP_INITIAL_WINDOW;
      alpha = bestScore - window;
      beta = bestScore + window;
    } else {
      window = INF;
      alpha = -INF;
      beta = INF;
    }

    Move iterationBestMove;
    int iterationBestScore = -INF;
    bool depth_completed = false;

    while (true) {
      Move moves[MAX_MOVES];
      int n = generateLegalMovesInto(moves);
      if (n == 0)
        return Move();

      if (bestMove != Move()) {
        for (int i = 0; i < n; i++) {
          if (moves[i] == bestMove) {
            std::swap(moves[0], moves[i]);
            break;
          }
        }
      }

      iterationBestMove = Move();
      iterationBestScore = -INF;
      int rootAlpha = alpha;
      bool aborted = false;

      bool first = true;
      for (int i = 0; i < n; i++) {
        const Move &move = moves[i];
        makeMove(move);
        int evaluation;
        if (first) {
          evaluation =
              -search(depth - 1, 1, -beta, -rootAlpha, true, true);
        } else {
          evaluation =
              -search(depth - 1, 1, -rootAlpha - 1, -rootAlpha, true, false);
          if (!time_up_flag && evaluation > rootAlpha && evaluation < beta) {
            evaluation =
                -search(depth - 1, 1, -beta, -rootAlpha, true, true);
          }
        }
        unmakeMove();

        if (time_up_flag) {
          aborted = true;
          break;
        }

        if (evaluation > iterationBestScore) {
          iterationBestScore = evaluation;
          iterationBestMove = move;
          if (evaluation > rootAlpha)
            rootAlpha = evaluation;
        }
        first = false;
      }

      if (aborted) {
        depth_completed = false;
        break;
      }

      if (iterationBestScore <= alpha) {
        window *= 2;
        alpha = std::max(-INF, iterationBestScore - window);
        if (window > 4 * ASP_INITIAL_WINDOW) {
          alpha = -INF;
          beta = INF;
        }
        continue;
      }
      if (iterationBestScore >= beta) {
        window *= 2;
        beta = std::min(INF, iterationBestScore + window);
        if (window > 4 * ASP_INITIAL_WINDOW) {
          alpha = -INF;
          beta = INF;
        }
        continue;
      }
      depth_completed = true;
      break;
    }

    if (depth_completed || depth == 1) {
      bestMove = iterationBestMove;
      bestScore = iterationBestScore;
      last_search_depth = depth;
      last_score = bestScore;

      search_progress.completed_depth.store(depth, std::memory_order_relaxed);
      search_progress.score_cp.store(bestScore, std::memory_order_relaxed);
      search_progress.hash_hits.store(searchStats.hash_hits,
                                      std::memory_order_relaxed);
      search_progress.fail_high.store(searchStats.fail_high,
                                      std::memory_order_relaxed);
      search_progress.fail_high_first.store(searchStats.fail_high_first,
                                            std::memory_order_relaxed);

      {
        IterationInfo info;
        info.depth = depth;
        info.score_cp = bestScore;
        info.nodes = (uint64_t)searchStats.nodes;
        info.time_ms = SDL_GetTicks() - start_time;
        info.pv = moveToUCI(bestMove);
        info.hash_hit_pct = safe_pct(searchStats.hash_hits, searchStats.nodes);
        info.fail_high_first_pct =
            safe_pct(searchStats.fail_high_first, searchStats.fail_high);
        info.effective_branching_x100 =
            searchStats.nodes == 0
                ? 0
                : (int)((uint64_t)searchStats.moves_searched * 100 /
                        searchStats.nodes);
        std::lock_guard<std::mutex> lk(iteration_log_mutex);
        iteration_log.push_back(std::move(info));
      }
    }

    if (std::abs(bestScore) > MATE_BOUND)
      break;
  }

  search_progress.active.store(false, std::memory_order_relaxed);
  return bestMove;
}

Move ChessEngine::parseMoveString(const std::string &moveStr) {
  if (moveStr.length() < 4)
    return Move();

  Square from =
      static_cast<Square>((moveStr[0] - 'a') + 8 * (moveStr[1] - '1'));
  Square to = static_cast<Square>((moveStr[2] - 'a') + 8 * (moveStr[3] - '1'));

  std::vector<Move> moves = generateLegalMoves();
  for (const Move &move : moves) {
    if (move.from() == from && move.to() == to) {
      if (moveStr.length() > 4) {
        char promo = moveStr[4];
        MoveFlags f = move.flags();
        if (promo == 'n' && (f == PR_KNIGHT || f == PC_KNIGHT))
          return move;
        if (promo == 'b' && (f == PR_BISHOP || f == PC_BISHOP))
          return move;
        if (promo == 'r' && (f == PR_ROOK || f == PC_ROOK))
          return move;
        if (promo == 'q' && (f == PR_QUEEN || f == PC_QUEEN))
          return move;
      } else {
        MoveFlags f = move.flags();
        if (f >= PR_KNIGHT && f <= PR_QUEEN)
          continue;
        if (f >= PC_KNIGHT && f <= PC_QUEEN)
          continue;
        return move;
      }
    }
  }
  return Move();
}
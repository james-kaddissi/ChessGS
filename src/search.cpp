#include "engine.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>

static constexpr int MATE_SCORE = 100000;
static constexpr int INF = 1000000;
static constexpr int MATE_BOUND = MATE_SCORE - MAX_PLY;

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

void ChessEngine::ttStore(uint64_t key, int depth, int score, TTBound bound,
                          Move bestMove, int ply) {
  TTEntry &e = tt[key & TT_MASK];
  if (e.key == key && e.depth > depth && e.bound == TT_EXACT &&
      bound != TT_EXACT) {
    return;
  }
  e.key = key;
  e.depth = static_cast<int8_t>(std::max(-128, std::min(127, depth)));
  e.score = static_cast<int16_t>(scoreToTT(score, ply));
  e.bound = static_cast<uint8_t>(bound);
  e.bestMove = bestMove;
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
    captype = PAWN; // safety fallback

  return vs[captype] * 10 - as[attackertype];
}

std::vector<ScoredMove> ChessEngine::orderMoves(const std::vector<Move> &moves,
                                                int ply, Move ttMove) {
  std::vector<ScoredMove> scored;
  scored.reserve(moves.size());
  Color side = position.turn();

  for (const Move &move : moves) {
    int score = 0;

    if (ttMove != Move() && move == ttMove) {
      score = 1'000'000;
    } else if (move.is_capture()) {
      score = 200'000 + getCaptureScore(move);
    } else if (move.flags() >= PR_KNIGHT && move.flags() <= PR_QUEEN) {
      score = 150'000 + (move.flags() - PR_KNIGHT) * 100;
    } else if (ply < MAX_PLY && move == killer_moves[ply][0]) {
      score = 100'000;
    } else if (ply < MAX_PLY && move == killer_moves[ply][1]) {
      score = 90'000;
    } else {
      score = history_table[side][move.from()][move.to()];
    }
    scored.push_back({move, score});
  }

  std::sort(scored.begin(), scored.end(),
            [](const ScoredMove &a, const ScoredMove &b) {
              return a.score > b.score;
            });
  return scored;
}

void ChessEngine::clearTables() {
  std::memset(history_table, 0, sizeof(history_table));
  clearKillers();
  std::memset(tt.data(), 0, tt.size() * sizeof(TTEntry));
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

int ChessEngine::search(int depth, int ply, int alpha, int beta,
                        bool nullPrune) {
  if (checkTimeUp())
    return alpha;
  searchStats.nodes++;

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
  if (ttProbe(hash, depth, alpha, beta, ply, ttScore, ttMove)) {
    searchStats.hash_used++;
    searchStats.hash_hits++;
    return ttScore;
  }

  if (depth <= 0) {
    return quiescence_search(alpha, beta);
  }

  bool inCheck = isInCheck(getSideToMove());

  if (nullPrune && depth >= 3 && !inCheck && game_phase() > 0) {
    Square savedEp = position.history[position.game_ply].epsq;
    Bitboard savedEntry = position.history[position.game_ply].entry;

    position.flip_side_hash();
    position.xor_ep_hash(savedEp); 
    position.side_to_play = ~position.side_to_play;
    position.game_ply++;
    position.history[position.game_ply] = UndoInfo();
    position.history[position.game_ply].entry =
        savedEntry;
    position.history[position.game_ply].epsq = NO_SQ;

    repetition_history.push_back(position.get_hash());

    int score = -search(depth - 3, ply + 1, -beta, -beta + 1, false);

    repetition_history.pop_back();
    position.game_ply--;
    position.side_to_play = ~position.side_to_play;
    position.flip_side_hash();
    position.xor_ep_hash(savedEp);

    if (time_up_flag)
      return alpha;

    if (score >= beta) {
      searchStats.null_prunes++;
      if (score >= MATE_BOUND)
        score = beta;
      return score;
    }
  }

  int old_alpha = alpha;
  std::vector<Move> moves = generateLegalMoves();
  if (moves.empty()) {
    if (inCheck)
      return -MATE_SCORE + ply; 
    return 0;                   
  }

  std::vector<ScoredMove> scoredMoves = orderMoves(moves, ply, ttMove);
  Move currentBestMove;
  bool first_move = true;
  int bestScore = -INF;

  for (const ScoredMove &sm : scoredMoves) {
    makeMove(sm.move);
    searchStats.moves_searched++;
    int evaluation = -search(depth - 1, ply + 1, -beta, -alpha, true);
    unmakeMove();

    if (time_up_flag)
      return alpha;

    if (evaluation > bestScore) {
      bestScore = evaluation;
      currentBestMove = sm.move;
    }

    if (evaluation >= beta) {
      searchStats.fail_high++;
      if (first_move)
        searchStats.fail_high_first++;
      if (!sm.move.is_capture()) {
        updateKillerMoves(sm.move, ply);
        updateHistoryTable(sm.move, depth, position.turn());
      }
      ttStore(hash, depth, evaluation, TT_LOWER, sm.move, ply);
      return evaluation;
    }

    if (evaluation > alpha) {
      alpha = evaluation;
    }
    first_move = false;
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

  int stand_pat = eval();

  if (stand_pat >= beta)
    return stand_pat;
  if (stand_pat > alpha)
    alpha = stand_pat;

  std::vector<Move> moves = generateLegalMoves();
  std::vector<Move> captures;
  captures.reserve(16);
  for (const Move &m : moves) {
    if (m.is_capture())
      captures.push_back(m);
  }
  std::sort(captures.begin(), captures.end(),
            [this](const Move &a, const Move &b) {
              return getCaptureScore(a) > getCaptureScore(b);
            });

  constexpr int DELTA_MARGIN = 200;
  static const int piece_val[7] = {100, 300, 300, 500, 900, 0, 0};

  for (const Move &move : captures) {
    int capValue = 0;
    if (move.flags() == EN_PASSANT) {
      capValue = piece_val[PAWN];
    } else {
      Color cc;
      PieceType ct = getPieceAt(move.to(), cc);
      if (ct < 6)
        capValue = piece_val[ct];
    }
    bool is_promo = move.flags() >= PC_KNIGHT && move.flags() <= PC_QUEEN;
    if (is_promo || (move.flags() >= PR_KNIGHT && move.flags() <= PR_QUEEN)) {
      capValue += piece_val[QUEEN] - piece_val[PAWN];
    }
    if (stand_pat + capValue + DELTA_MARGIN < alpha)
      continue;

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

  start_time = SDL_GetTicks();
  allocated_time_ms = 0; 
  time_up_flag = false;

  clearKillers();

  Move bestMove;
  int bestScore = -INF;

  for (int depth = 1; depth <= maxDepth; depth++) {
    int alpha = -INF;
    int beta = INF;
    Move iterationBestMove;
    int iterationBestScore = -INF;

    std::vector<Move> moves = generateLegalMoves();
    if (moves.empty())
      return Move();

    if (bestMove != Move()) {
      for (size_t i = 0; i < moves.size(); i++) {
        if (moves[i] == bestMove) {
          std::swap(moves[0], moves[i]);
          break;
        }
      }
    }

    for (const Move &move : moves) {
      makeMove(move);
      int evaluation = -search(depth - 1, 1, -beta, -alpha, true);
      unmakeMove();

      if (evaluation > iterationBestScore) {
        iterationBestScore = evaluation;
        iterationBestMove = move;
        if (evaluation > alpha)
          alpha = evaluation;
      }
    }

    bestMove = iterationBestMove;
    bestScore = iterationBestScore;
    last_search_depth = depth;
    last_score = bestScore;

    std::cout << "info depth " << depth << " score cp " << bestScore << " pv "
              << moveToUCI(bestMove) << std::endl;

    if (std::abs(bestScore) > MATE_BOUND)
      break;
  }

  return bestMove;
}

Move ChessEngine::getBestMoveWithTime(int time_ms) {
  Move bookMove = getOpeningBookMove();
  if (bookMove != Move())
    return bookMove;

  start_time = SDL_GetTicks();
  allocated_time_ms = time_ms;
  time_up_flag = false;

  clearKillers();

  Move bestMove;
  int bestScore = -INF;

  for (int depth = 1; depth <= MAX_B_DEPTH; depth++) {
    if (time_up_flag)
      break;

    Uint32 elapsed = SDL_GetTicks() - start_time;
    if (depth > 1 && elapsed > (Uint32)(time_ms / 2))
      break;

    int alpha = -INF;
    int beta = INF;
    Move iterationBestMove;
    int iterationBestScore = -INF;

    std::vector<Move> moves = generateLegalMoves();
    if (moves.empty())
      return Move();

    if (bestMove != Move()) {
      for (size_t i = 0; i < moves.size(); i++) {
        if (moves[i] == bestMove) {
          std::swap(moves[0], moves[i]);
          break;
        }
      }
    }

    bool depth_completed = true;
    for (const Move &move : moves) {
      makeMove(move);
      int evaluation = -search(depth - 1, 1, -beta, -alpha, true);
      unmakeMove();

      if (time_up_flag) {
        depth_completed = false;
        break;
      }

      if (evaluation > iterationBestScore) {
        iterationBestScore = evaluation;
        iterationBestMove = move;
        if (evaluation > alpha)
          alpha = evaluation;
      }
    }

    if (depth_completed || depth == 1) {
      bestMove = iterationBestMove;
      bestScore = iterationBestScore;
      last_search_depth = depth;
      last_score = bestScore;

      Uint32 e = SDL_GetTicks() - start_time;
      std::cout << "info depth " << depth << " score cp " << bestScore
                << " time " << e << " nodes " << searchStats.nodes << " pv "
                << moveToUCI(bestMove) << std::endl;
    }

    if (std::abs(bestScore) > MATE_BOUND)
      break;
  }

  std::cout << "info time " << (SDL_GetTicks() - start_time) << std::endl;
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
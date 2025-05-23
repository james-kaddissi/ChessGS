#pragma once

#include "bitboard.h"
#include "chess_types.h"
#include <SDL2/SDL.h>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

struct Score {
  int mg;
  int eg;

  Score(int mg_ = 0, int eg_ = 0) : mg(mg_), eg(eg_) {}

  Score operator+(const Score &other) const {
    return Score(mg + other.mg, eg + other.eg);
  }

  Score &operator+=(const Score &other) {
    mg += other.mg;
    eg += other.eg;
    return *this;
  }

  Score operator-(const Score &other) const {
    return Score(mg - other.mg, eg - other.eg);
  }

  Score operator*(int value) const { return Score(mg * value, eg * value); }

  Score operator/(int value) const { return Score(mg / value, eg / value); }
};

struct ScoredMove {
  Move move;
  int score;
};

enum TTBound : uint8_t { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
  uint64_t key;
  Move bestMove;
  int16_t score;
  int8_t depth;
  uint8_t bound;
  uint8_t age;
};

struct OpeningBookMove {
  uint64_t hash;
  int from;
  int to;
  int promo; 
  int weight;
};

#define MAX_PLY 128
#define MAX_Q_DEPTH 8
#define MAX_B_DEPTH 64
#define MAX_MOVES 256

struct AnalysisResult {
  int64_t nodes;
  double time_ms;
  int depth_reached;
  std::string best_move;
  int score;
};

struct SearchStatistics {
  int64_t nodes;
  int64_t qnodes;
  int hash_hits;
  int hash_used;
  int null_prunes;
  int fail_high_first;
  int fail_high;
  int moves_searched;
};

struct MatchResult {
  int white_wins = 0;
  int black_wins = 0;
  int draws = 0;

  void print() {
    int total = white_wins + black_wins + draws;
    if (total == 0)
      return;
    double score = (white_wins + 0.5 * draws) / total;

    std::cout << "Match results:" << std::endl;
    std::cout << "  White wins: " << white_wins << std::endl;
    std::cout << "  Black wins: " << black_wins << std::endl;
    std::cout << "  Draws: " << draws << std::endl;
    std::cout << "  Total games: " << total << std::endl;
    std::cout << "  Score: " << (score * 100) << "%" << std::endl;
  }
};

struct TestPosition {
  std::string fen;
  std::string best_move;
  std::string description;
};

class ChessEngine {
public:
  struct SearchProgress {
    std::atomic<int> depth{0};
    std::atomic<int> completed_depth{0};
    std::atomic<int> score_cp{0};
    std::atomic<uint64_t> nodes{0};
    std::atomic<uint64_t> qnodes{0};
    std::atomic<uint64_t> hash_hits{0};
    std::atomic<uint64_t> fail_high{0};
    std::atomic<uint64_t> fail_high_first{0};
    std::atomic<uint32_t> start_ms{0};
    std::atomic<bool> active{false};
  };

  struct IterationInfo {
    int depth;
    int score_cp;
    uint64_t nodes;
    uint32_t time_ms;
    std::string pv;
    int hash_hit_pct;
    int fail_high_first_pct;
    int effective_branching_x100;
  };
  ChessEngine();
  ~ChessEngine();

  // setup
  void resetToStartingPosition();
  PieceType getPieceAt(Square sq, Color &color);

  // move gen
  std::vector<Move> generateLegalMoves();
  int generateLegalMovesInto(Move *buf);
  bool makeMove(const Move &move);
  void unmakeMove();

  // game state
  Color getSideToMove() const;
  bool isInCheck(Color side) const;
  bool isCheckmate() const;
  bool isStalemate() const;
  bool isRepetition() const;
  bool isDrawByInsufficientMaterial() const;

  // formatting
  std::string moveToString(const Move &move) const;
  std::string moveToUCI(const Move &move) const;

  const SearchProgress &progress() const { return search_progress; }
  std::vector<IterationInfo> drainIterationLog();

  void stop() { time_up_flag = true; }

  // evaluation (evaluation.cpp)
  Bitboard getFriendlyPieces(Color color) const;
  int eval();
  Score evaluate_color(Color color);
  int count_material(Color color);
  int non_pawn_material(Color color);
  int game_phase();
  Score evalPawns(Color color);
  Score evalKnights(Color color);
  Score evalBishops(Color color);
  Score evalRooks(Color color);
  Score evalQueens(Color color);
  Score evalKing(Color color);
  Score evalPawnStructure(Color color);
  Score evalKingVulnerability(Color color);
  Score evalKnightMobility(Square sq, Color color, Bitboard poss);
  Score evalEndgameTerms(Color color); 

  // search (search.cpp)
  int search(int depth, int ply, int alpha, int beta, bool nullPrune,
             bool isPv);
  int quiescence_search(int alpha, int beta, int qdepth = 0);
  int getCaptureScore(const Move &move);
  int see(const Move &move);
  Move getBestMove(int depth);
  Move getBestMoveWithTime(int time_ms);
  void orderMovesInto(const Move *moves, int n, int ply, Move ttMove,
                      ScoredMove *out);
  Move parseMoveString(const std::string &moveStr);
  void clearTables();
  void clearKillers();
  void updateKillerMoves(const Move &move, int ply);
  void updateHistoryTable(const Move &move, int depth, Color side);
  bool checkTimeUp();

  // book (book.cpp)
  void loadOpeningBook(const std::string &filename);
  bool isPolyglotFormat(const std::string &filename);
  Move resolvePolyglotMove(int from, int to, int promo);
  Move getOpeningBookMove();

  // tools
  std::vector<AnalysisResult>
  runAnalysis(const std::vector<std::string> &positions,
              int time_per_position_ms);
  void resetSearchStats();
  void printSearchStats();
  void perftDivide(int depth);
  uint64_t perft(int depth);
  void testPerft();
  MatchResult selfPlayGames(int games, int depth, bool useTimeControl,
                            int msPerMove, bool useOpeningBook);
  void runTestSuite(const std::string &filename);
  void uciLoop();

private:
  PositionManager position;

  static constexpr size_t TT_SIZE = 1u << 22;
  static constexpr size_t TT_MASK = TT_SIZE - 1;
  std::vector<TTEntry> tt;
  uint8_t tt_age; 

  void ttStore(uint64_t key, int depth, int score, TTBound bound, Move bestMove,
               int ply);
  bool ttProbe(uint64_t key, int depth, int alpha, int beta, int ply,
               int &score, Move &bestMove);

  static int scoreToTT(int score, int ply);
  static int scoreFromTT(int score, int ply);

  std::vector<Move> moveStack;

  std::vector<uint64_t> repetition_history;

  std::vector<OpeningBookMove> openingBook;

  int history_table[2][64][64];
  Move killer_moves[MAX_PLY][2];

  int lmr_reductions[64][64];
  void initLmrTable();

  SearchStatistics searchStats;
  uint64_t total_nodes;
  int last_search_depth;
  int last_score;

  // time control
  Uint32 start_time;
  int allocated_time_ms;
  bool time_up_flag;
  static constexpr int nodes_between_checks = 1024;

  SearchProgress search_progress;

  mutable std::mutex iteration_log_mutex;
  std::vector<IterationInfo> iteration_log;
};
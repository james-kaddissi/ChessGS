#pragma once

#include "chess_types.h"
#include "bitboard.h"
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct Score {
    int mg; // midgame
    int eg; // endgame

    Score(int mg_ = 0, int eg_ = 0) : mg(mg_), eg(eg_) {}

    Score operator+(const Score& other) const {
        return Score(mg + other.mg, eg + other.eg);
    }

    Score& operator+=(const Score& other) {
        mg += other.mg;
        eg += other.eg;
        return *this;
    }

    Score operator-(const Score& other) const {
        return Score(mg - other.mg, eg - other.eg);
    }

    Score operator*(int value) const {
        return Score(mg * value, eg * value);
    }

    Score operator/(int value) const {
        return Score(mg / value, eg / value);
    }
};

struct ScoredMove {
    Move move;
    int score;
};

struct TranspositionTableElement {
    uint64_t hash;
    int depth;
    int score;
    int flag;
    Move bestMove;
};

struct OpeningBookMove {
    uint64_t hash;
    Move move;
    int weight;
};


#define MAX_PLY 64

struct AnalysisResult {
    int64_t nodes;
    double time_ms;
    int depth_reached;
    std::string best_move;
    int score;
};

struct SearchStatistics{
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
        double score = (white_wins + 0.5 * draws) / total * 100;

        std::cout << "Match results:" << std::endl;
        std::cout << "  White wins: " << white_wins << std::endl;
        std::cout << "  Black wins: " << black_wins << std::endl;
        std::cout << "  Draws: " << draws << std::endl;
        std::cout << "  Total games: " << total << std::endl;
        std::cout << "  Score: " << score * 100 << "%" << std::endl;
    }
};

struct TestPosition {
    std::string fen;
    std::string best_move;
    std::string description;
};

#define MAX_Q_DEPTH 8
#define MAX_B_DEPTH 20

class ChessEngine {
public:
    ChessEngine();
    ~ChessEngine();
    
    // setup operations and board
    void resetToStartingPosition();
    PieceType getPieceAt(Square sq, Color& color);
    
    // generating moves from PositionManager
    std::vector<Move> generateLegalMoves();
    bool makeMove(const Move& move);
    void unmakeMove();
    // game state
    Color getSideToMove() const;
    bool isInCheck(Color side) const;
    bool isCheckmate() const;
    bool isStalemate() const;
    
    // formatting moves
    std::string moveToString(const Move& move) const;

    // evaluation functions
    Bitboard getFriendlyPieces(Color color) const;
    int eval();
    Score evaluate_color(Color color);
    int count_material(Color color);
    int game_phase();
    Score evalPawns(Color color);
    Score evalKnights(Color color);
    Score evalBishops(Color color);
    Score evalRooks(Color color);
    Score evalQueens(Color color);
    int search(int depth, int alpha, int beta, bool nullPrune);
    int quiescence_search(int alpha, int beta, int qdepth = 0);
    int getCaptureScore(const Move& move);
    Move getBestMove(int depth);
    Score evalPawnStructure(Color color);
    Score evalKingVulnerability(Color color);
    Score evalKnightMobility(Square sq, Color color, Bitboard poss);
    void loadOpeningBook(const std::string& filename);
    bool isPolygotFormat(const std::string& filename);
    Move polyglotMoveToMove(uint16_t moveData);
    Move getOpeningBookMove();
    bool inEndgame();
    int evalEndgame();
    std::vector<ScoredMove> orderMoves(const std::vector<Move>& moves);
    Move parseMoveString(const std::string& moveStr);
    void clearTables();
    void updateKillerMoves(const Move& move, int ply);
    void updateHistoryTable(const Move& move, int depth, Color side);
    Move getBestMoveWithTime(int time_ms);
    std::vector<AnalysisResult> runAnalysis(const std::vector<std::string>& positions, int time_per_position_ms);
    void resetSearchStats();
    void printSearchStats();
    void perftDivide(int depth);
    uint64_t perft(int depth);
    void testPerft();   
    MatchResult selfPlayGames(int games, int depth, bool useTimeControl, int msPerMove, bool useOpeningBook);
    void runTestSuite(const std::string& filename);
    void uciLoop();
    bool checkTimeUp();
    

private:
    PositionManager position;

    std::unordered_map<uint64_t, TranspositionTableElement> transpositionTable;

    std::vector<OpeningBookMove> openingBook;

    int history_table[2][64][64];
    Move killer_moves[MAX_PLY][2];
    int current_ply;

    SearchStatistics searchStats;
    uint64_t total_nodes;
    int last_search_depth;
    int last_score;

    Uint32 start_time;
    int allocated_time_ms;
    bool time_up_flag;
    const int nodes_between_checks = 4096;
}; 
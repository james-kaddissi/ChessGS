#include "engine.h"
#include "pst.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <SDL3/SDL.h>
#include <algorithm>

std::vector<ChessEngine::IterationInfo> ChessEngine::drainIterationLog() {
    std::lock_guard<std::mutex> lk(iteration_log_mutex);
    std::vector<IterationInfo> out;
    out.swap(iteration_log);
    return out;
}

ChessEngine::ChessEngine() : tt(TT_SIZE) {
    std::memset(tt.data(), 0, tt.size() * sizeof(TTEntry));
    tt_age = 0;

    last_score = 0;
    total_nodes = 0;
    last_search_depth = 0;
    start_time = 0;
    allocated_time_ms = 0;
    time_up_flag = false;

    std::memset(history_table, 0, sizeof(history_table));
    for (int i = 0; i < MAX_PLY; i++) {
        killer_moves[i][0] = Move();
        killer_moves[i][1] = Move();
    }
    initLmrTable();
    resetSearchStats();

    resetToStartingPosition();

    try {
        if (std::ifstream("book.bin").good()) {
            loadOpeningBook("book.bin");
        } else {
            openingBook.clear();
        }
    } catch (...) {
        std::cerr << "Warning: Error loading opening book." << std::endl;
        openingBook.clear();
    }
}

ChessEngine::~ChessEngine() {}

void ChessEngine::resetToStartingPosition() {
    PositionManager::set(DEFAULT_FEN, position);
    moveStack.clear();
    repetition_history.clear();
    repetition_history.push_back(position.get_hash());
}

PieceType ChessEngine::getPieceAt(Square sq, Color& color) {
    Piece piece = position.at(sq);
    if (piece == NO_PIECE) {
        color = WHITE;
        return NONE;
    }
    color = piece_color(piece);
    return piece_type(piece);
}

std::vector<Move> ChessEngine::generateLegalMoves() {
    std::vector<Move> moves;
    moves.reserve(64);
    if (position.turn() == WHITE) {
        MoveList<WHITE> list(position);
        for (const Move& move : list) moves.push_back(move);
    } else {
        MoveList<BLACK> list(position);
        for (const Move& move : list) moves.push_back(move);
    }
    return moves;
}

bool ChessEngine::makeMove(const Move& move) {
    moveStack.push_back(move);
    if (position.turn() == WHITE) {
        position.play<WHITE>(move);
    } else {
        position.play<BLACK>(move);
    }
    position.flip_side_hash();
    repetition_history.push_back(position.get_hash());
    return true;
}

void ChessEngine::unmakeMove() {
    if (moveStack.empty()) {
        std::cout << "Move stack is empty, nothing to undo.\n";
        return;
    }
    Move lastMove = moveStack.back();
    moveStack.pop_back();
    if (!repetition_history.empty()) repetition_history.pop_back();

    position.flip_side_hash();

    Color turnBeforeUndo = position.turn();
    if (turnBeforeUndo == WHITE) {
        position.undo<BLACK>(lastMove);
    } else {
        position.undo<WHITE>(lastMove);
    }
}

Color ChessEngine::getSideToMove() const {
    return position.turn();
}

bool ChessEngine::isInCheck(Color side) const {
    return (side == WHITE) ? position.in_check<WHITE>() : position.in_check<BLACK>();
}

bool ChessEngine::isCheckmate() const {
    PositionManager& p = const_cast<PositionManager&>(position);
    if (position.turn() == WHITE) {
        MoveList<WHITE> list(p);
        return position.in_check<WHITE>() && list.size() == 0;
    } else {
        MoveList<BLACK> list(p);
        return position.in_check<BLACK>() && list.size() == 0;
    }
}

bool ChessEngine::isStalemate() const {
    PositionManager& p = const_cast<PositionManager&>(position);
    if (position.turn() == WHITE) {
        MoveList<WHITE> list(p);
        return !position.in_check<WHITE>() && list.size() == 0;
    } else {
        MoveList<BLACK> list(p);
        return !position.in_check<BLACK>() && list.size() == 0;
    }
}

std::string ChessEngine::moveToString(const Move& move) const {
    std::stringstream ss;
    Square from = move.from();
    Square to = move.to();
    Piece piece = position.at(from);
    PieceType pieceType = piece_type(piece);

    if (pieceType != PAWN && pieceType != NONE) {
        ss << "NBRQK"[pieceType - 1];
    }
    ss << SQUARE_STR[from] << SQUARE_STR[to];

    MoveFlags f = move.flags();
    if (f == PR_KNIGHT || f == PC_KNIGHT) ss << "n";
    else if (f == PR_BISHOP || f == PC_BISHOP) ss << "b";
    else if (f == PR_ROOK || f == PC_ROOK) ss << "r";
    else if (f == PR_QUEEN || f == PC_QUEEN) ss << "q";

    return ss.str();
}

std::string ChessEngine::moveToUCI(const Move& move) const {
    std::string s;
    s += static_cast<char>('a' + file_of(move.from()));
    s += static_cast<char>('1' + rank_of(move.from()));
    s += static_cast<char>('a' + file_of(move.to()));
    s += static_cast<char>('1' + rank_of(move.to()));

    MoveFlags f = move.flags();
    if (f == PR_KNIGHT || f == PC_KNIGHT) s += 'n';
    else if (f == PR_BISHOP || f == PC_BISHOP) s += 'b';
    else if (f == PR_ROOK || f == PC_ROOK) s += 'r';
    else if (f == PR_QUEEN || f == PC_QUEEN) s += 'q';
    return s;
}

void ChessEngine::resetSearchStats() {
    searchStats = {0, 0, 0, 0, 0, 0, 0, 0};
    total_nodes = 0;
}

std::vector<AnalysisResult> ChessEngine::runAnalysis(const std::vector<std::string>& positions, int time_per_position_ms) {
    std::vector<AnalysisResult> results;
    for (const std::string& fen : positions) {
        PositionManager::set(fen, position);
        moveStack.clear();
        repetition_history.clear();
        repetition_history.push_back(position.get_hash());

        resetSearchStats();
        auto start_time = std::chrono::high_resolution_clock::now();
        Move best_move = getBestMoveWithTime(time_per_position_ms);
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        AnalysisResult r;
        r.nodes = searchStats.nodes;
        r.time_ms = elapsed;
        r.depth_reached = last_search_depth;
        r.best_move = moveToUCI(best_move);
        r.score = last_score;
        results.push_back(r);
    }
    return results;
}

void ChessEngine::printSearchStats() {
    double branching = 0;
    if (searchStats.moves_searched > 0 && searchStats.nodes > 0)
        branching = (double)searchStats.moves_searched / searchStats.nodes;
    double hash_rate = 0;
    if (searchStats.nodes > 0)
        hash_rate = (double)searchStats.hash_hits / searchStats.nodes * 100.0;
    double cutoff_rate = 0;
    if (searchStats.fail_high > 0)
        cutoff_rate = (double)searchStats.fail_high_first / searchStats.fail_high * 100.0;

    std::cout << "Search Stats:\n"
              << " - Nodes: " << searchStats.nodes
              << " (" << searchStats.qnodes << " qnodes)\n"
              << " - Avg branching factor: " << std::fixed << std::setprecision(2) << branching << "\n"
              << " - Hash hits: " << searchStats.hash_hits << " (" << hash_rate << "%)\n"
              << " - Fail-high first: " << searchStats.fail_high_first << "/" << searchStats.fail_high
              << " (" << cutoff_rate << "%)\n"
              << " - Null prunes: " << searchStats.null_prunes << "\n"
              << " - Hash used: " << searchStats.hash_used << "\n"
              << " - Moves searched: " << searchStats.moves_searched << "\n";
}

uint64_t ChessEngine::perft(int depth) {
    if (depth == 0) return 1;
    std::vector<Move> moves = generateLegalMoves();
    if (depth == 1) return moves.size();
    uint64_t nodes = 0;
    for (const Move& move : moves) {
        makeMove(move);
        nodes += perft(depth - 1);
        unmakeMove();
    }
    return nodes;
}

void ChessEngine::perftDivide(int depth) {
    std::vector<Move> moves = generateLegalMoves();
    uint64_t total = 0;
    for (const Move& move : moves) {
        makeMove(move);
        uint64_t count = (depth > 1) ? perft(depth - 1) : 1;
        unmakeMove();
        std::cout << moveToUCI(move) << ": " << count << std::endl;
        total += count;
    }
    std::cout << "\nTotal: " << total << std::endl;
}

void ChessEngine::testPerft() {
    struct PerftTest { std::string fen; int depth; uint64_t expected; };
    std::vector<PerftTest> tests = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487}
    };
    int passed = 0;
    for (const PerftTest& test : tests) {
        PositionManager::set(test.fen, position);
        moveStack.clear();
        repetition_history.clear();
        repetition_history.push_back(position.get_hash());

        auto start = std::chrono::high_resolution_clock::now();
        uint64_t result = perft(test.depth);
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double nps = (time_ms > 0) ? result * 1000.0 / time_ms : 0;
        bool ok = (result == test.expected);
        std::cout << "FEN: " << test.fen << "\n"
                  << "Depth: " << test.depth
                  << "  Nodes: " << result
                  << "  Expected: " << test.expected
                  << "  Time: " << time_ms << "ms"
                  << "  NPS: " << (uint64_t)nps
                  << "  " << (ok ? "PASS" : "FAIL") << "\n"
                  << "----------\n";
        if (ok) passed++;
    }
    std::cout << "Passed " << passed << "/" << tests.size() << " tests\n";
}

MatchResult ChessEngine::selfPlayGames(int games, int depth, bool useTimeControl, int msPerMove, bool useOpeningBook) {
    MatchResult result;

    for (int i = 0; i < games; i++) {
        resetToStartingPosition();
        int winner = -1;
        int half_moves = 0;

        while (winner == -1 && half_moves < 200) {
            Move move;

            if (useOpeningBook && half_moves < 20) {
                move = getOpeningBookMove();
                if (move == Move()) {
                    move = useTimeControl ? getBestMoveWithTime(msPerMove) : getBestMove(depth);
                }
            } else {
                move = useTimeControl ? getBestMoveWithTime(msPerMove) : getBestMove(depth);
            }

            if (move == Move()) {
                if (isInCheck(getSideToMove())) {
                    winner = (getSideToMove() == WHITE) ? 2 : 1;
                } else {
                    winner = 0;
                }
                break;
            }

            makeMove(move);
            half_moves++;

            if (isCheckmate()) {
                winner = (getSideToMove() == WHITE) ? 2 : 1;
            } else if (isStalemate() || isDrawByInsufficientMaterial() || isRepetition() || half_moves >= 200) {
                winner = 0;
            }
        }

        if (winner == 0) result.draws++;
        else if (winner == 1) result.white_wins++;
        else result.black_wins++;

        std::cout << "Game " << (i + 1) << ": ";
        if (winner == 0) std::cout << "Draw";
        else if (winner == 1) std::cout << "White wins";
        else std::cout << "Black wins";
        std::cout << " (" << half_moves << " half-moves)" << std::endl;
    }

    return result;
}

void ChessEngine::runTestSuite(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open test suite file" << std::endl;
        return;
    }
    std::string line;
    std::vector<TestPosition> positions;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        TestPosition pos;
        std::getline(iss, pos.fen, ';');
        std::getline(iss, pos.best_move, ';');
        std::getline(iss, pos.description);
        positions.push_back(pos);
    }
    int correct = 0;
    int total = (int)positions.size();
    for (const TestPosition& pos : positions) {
        PositionManager::set(pos.fen, position);
        moveStack.clear();
        repetition_history.clear();
        repetition_history.push_back(position.get_hash());

        Move best_move = getBestMove(8);
        std::string move_str = moveToUCI(best_move);
        bool is_correct = (move_str == pos.best_move);
        if (is_correct) correct++;
        std::cout << "Position: " << pos.description << "\n"
                  << "FEN: " << pos.fen << "\n"
                  << "Expected: " << pos.best_move << "\n"
                  << "Engine: " << move_str << "\n"
                  << (is_correct ? "CORRECT" : "WRONG") << "\n----------\n";
    }
    if (total > 0) {
        std::cout << "Test results: " << correct << "/" << total
                  << " (" << (correct * 100.0 / total) << "%)\n";
    }
}

void ChessEngine::uciLoop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "quit") {
            break;
        } else if (token == "uci") {
            std::cout << "id name ChessGS\n"
                      << "id author James Kaddissi\n"
                      << "option name Hash type spin default 64 min 1 max 1024\n"
                      << "option name Threads type spin default 1 min 1 max 8\n"
                      << "uciok\n";
        } else if (token == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (token == "ucinewgame") {
            clearTables();
            resetToStartingPosition();
        } else if (token == "position") {
            iss >> token;
            if (token == "startpos") {
                resetToStartingPosition();
                iss >> token;
            } else if (token == "fen") {
                std::string fen;
                while (iss >> token && token != "moves") {
                    fen += token + " ";
                }
                PositionManager::set(fen, position);
                moveStack.clear();
                repetition_history.clear();
                repetition_history.push_back(position.get_hash());
            }

            if (token == "moves") {
                std::string moveStr;
                while (iss >> moveStr) {
                    Move m = parseMoveString(moveStr);
                    if (m != Move()) makeMove(m);
                }
            }
        } else if (token == "go") {
            int depth = 6;
            int movetime = 0;
            while (iss >> token) {
                if (token == "depth")    iss >> depth;
                else if (token == "movetime") iss >> movetime;
            }
            Move best_move = (movetime > 0)
                ? getBestMoveWithTime(movetime)
                : getBestMove(depth);
            std::cout << "bestmove " << moveToUCI(best_move) << std::endl;
        }
    }
}
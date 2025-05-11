#include <iostream>
#include <chrono>
#include <string>
#include "lookup_tables.h"
#include "bitboard.h"
#include "chess_types.h"
#include "window.h"
#include "engine.h"

void printUsage() {
    std::cout << "ChessGS - Chess Game and Engine" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  gui                   - Start the GUI" << std::endl;
    std::cout << "  uci                   - Start UCI mode" << std::endl;
    std::cout << "  perft [depth]         - Run Perft test to specified depth" << std::endl;
    std::cout << "  testsuite [filename]  - Run test suite from file" << std::endl;
    std::cout << "  selfplay [n] [depth]  - Run n self-play games at specified depth" << std::endl;
    std::cout << "  benchmark            - Run benchmark" << std::endl;
}

void runBenchmark() {
    ChessEngine engine;
    
    std::vector<std::string> positions = {
        "r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
    };
    
    std::cout << "Running benchmark..." << std::endl;
    engine.resetSearchStats();
    
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<AnalysisResult> results = engine.runAnalysis(positions, 3000);
    auto end = std::chrono::high_resolution_clock::now();
    
    double total_time = std::chrono::duration<double>(end - start).count();
    uint64_t total_nodes = 0;
    
    std::cout << "\nBenchmark Results:" << std::endl;
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "Position " << (i+1) << ": " << results[i].best_move 
                  << " (depth " << results[i].depth_reached 
                  << ", score " << results[i].score 
                  << ", " << results[i].nodes << " nodes, " 
                  << results[i].time_ms << " ms)" << std::endl;
        total_nodes += results[i].nodes;
    }
    
    std::cout << "\nTotal time: " << total_time << " seconds" << std::endl;
    std::cout << "Total nodes: " << total_nodes << std::endl;
    std::cout << "Nodes per second: " << (int)(total_nodes / total_time) << std::endl;
    
    engine.printSearchStats();
}

int main(int argc, char* argv[]) {
	initialise_all_databases();
	zobrist::initialise_zobrist_keys();
	
    if (argc < 2) {
        Window window("ChessGS - Chess Game and Engine", 1024, 768);
        if (!window.Initialize()) {
            std::cerr << "Failed to initialize window" << std::endl;
            return 1;
        }
        window.RenderLoop();
    } else {
        std::string command = argv[1];
        
        if (command == "gui") {
            Window window("ChessGS - Chess Game and Engine", 1024, 768);
            if (!window.Initialize()) {
                std::cerr << "Failed to initialize window" << std::endl;
                return 1;
            }
            window.RenderLoop();
        } 
        else if (command == "uci") {
            ChessEngine engine;
            engine.uciLoop();
        } 
        else if (command == "perft") {
            int depth = 5;
            if (argc > 2) {
                depth = std::stoi(argv[2]);
            }
            ChessEngine engine;
            engine.perftDivide(depth);
        } 
        else if (command == "testsuite") {
            if (argc < 3) {
                std::cerr << "Error: No test suite file specified" << std::endl;
                return 1;
            }
            ChessEngine engine;
            engine.runTestSuite(argv[2]);
        } 
        else if (command == "selfplay") {
            int games = 10;
            int depth = 5;
			int useTimeControl = false;
			int msPerMove = 1000;
			bool useOpeningBook = false;
            
            if (argc > 2) games = std::stoi(argv[2]);
            if (argc > 3) depth = std::stoi(argv[3]);
            if (argc > 4) {
				std::string timeOption = argv[4];
        		useTimeControl = (timeOption == "time");
			}
			if (argc > 5) msPerMove = std::stoi(argv[5]);
			if (argc > 6) {
				std::string bookOption = argv[6];
				useOpeningBook = (bookOption == "book");
			}
            ChessEngine engine;
			std::cout << "Running " << games << " self-play games with " << (useTimeControl ? (std::to_string(msPerMove) + "ms per move") : ("depth " + std::to_string(depth))) << (useOpeningBook ? " using opening book" : "") << std::endl;
            MatchResult result = engine.selfPlayGames(games, depth, true, 1000, false);
            result.print();
        } 
        else if (command == "benchmark") {
            runBenchmark();
        } 
        else {
            printUsage();
        }
    }
    
    return 0;
}
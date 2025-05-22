#include "engine.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>

bool ChessEngine::isPolyglotFormat(const std::string& filename) {
    return filename.size() > 4 && filename.substr(filename.size() - 4) == ".bin";
}

void ChessEngine::loadOpeningBook(const std::string& filename) {
    openingBook.clear();

    if (isPolyglotFormat(filename)) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open opening book file: " << filename << std::endl;
            return;
        }

        const int ENTRY_SIZE = 16;
        char buffer[ENTRY_SIZE];

        while (file.read(buffer, ENTRY_SIZE)) {
            uint64_t key = 0;
            for (int i = 0; i < 8; i++) {
                key |= static_cast<uint64_t>(static_cast<unsigned char>(buffer[i])) << (8 * (7 - i));
            }

            uint16_t moveData = 0;
            moveData |= static_cast<uint16_t>(static_cast<unsigned char>(buffer[8])) << 8;
            moveData |= static_cast<uint16_t>(static_cast<unsigned char>(buffer[9]));

            uint16_t weight = 0;
            weight |= static_cast<uint16_t>(static_cast<unsigned char>(buffer[10])) << 8;
            weight |= static_cast<uint16_t>(static_cast<unsigned char>(buffer[11]));

            int from = (moveData >> 6) & 0x3F;
            int to   = moveData & 0x3F;
            int promo = (moveData >> 12) & 0x7;

            openingBook.push_back({key, from, to, promo, weight});
        }
        file.close();
    } else {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open opening book file: " << filename << std::endl;
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            uint64_t hash;
            std::string moveStr;
            int weight = 1;
            iss >> std::hex >> hash >> moveStr;
            iss >> std::dec >> weight;
            if (moveStr.length() < 4) continue;
            int from_file = moveStr[0] - 'a';
            int from_rank = moveStr[1] - '1';
            int to_file   = moveStr[2] - 'a';
            int to_rank   = moveStr[3] - '1';
            int from = from_rank * 8 + from_file;
            int to   = to_rank   * 8 + to_file;
            int promo = 0;
            if (moveStr.length() > 4) {
                switch (moveStr[4]) {
                    case 'n': promo = 1; break;
                    case 'b': promo = 2; break;
                    case 'r': promo = 3; break;
                    case 'q': promo = 4; break;
                }
            }
            openingBook.push_back({hash, from, to, promo, weight});
        }
        file.close();
    }

    std::sort(openingBook.begin(), openingBook.end(),
              [](const OpeningBookMove& a, const OpeningBookMove& b) {
                  return a.hash < b.hash;
              });

    std::cout << "Loaded " << openingBook.size() << " opening book positions" << std::endl;
}

Move ChessEngine::resolvePolyglotMove(int from, int to, int promo) {
    Square fromSq = static_cast<Square>(from);
    Square toSq   = static_cast<Square>(to);

    if (fromSq == E1 && toSq == H1) toSq = G1;
    else if (fromSq == E1 && toSq == A1) toSq = C1;
    else if (fromSq == E8 && toSq == H8) toSq = G8;
    else if (fromSq == E8 && toSq == A8) toSq = C8;

    std::vector<Move> legalMoves = generateLegalMoves();
    for (const Move& move : legalMoves) {
        if (move.from() == fromSq && move.to() == toSq) {
            if (promo > 0) {
                MoveFlags f = move.flags();
                if (promo == 1 && (f == PR_KNIGHT || f == PC_KNIGHT)) return move;
                if (promo == 2 && (f == PR_BISHOP || f == PC_BISHOP)) return move;
                if (promo == 3 && (f == PR_ROOK   || f == PC_ROOK))   return move;
                if (promo == 4 && (f == PR_QUEEN  || f == PC_QUEEN))  return move;
            } else {
                MoveFlags f = move.flags();
                if (f >= PR_KNIGHT && f <= PR_QUEEN) continue;
                if (f >= PC_KNIGHT && f <= PC_QUEEN) continue;
                return move;
            }
        }
    }
    return Move();
}

Move ChessEngine::getOpeningBookMove() {
    if (openingBook.empty()) return Move();

    uint64_t currentHash = position.get_hash();

    auto compareByHash = [](const OpeningBookMove& a, const OpeningBookMove& b) {
        return a.hash < b.hash;
    };
    OpeningBookMove searchKey{currentHash, 0, 0, 0, 0};
    auto range = std::equal_range(openingBook.begin(), openingBook.end(), searchKey, compareByHash);

    if (range.first == range.second) return Move();

    int totalWeight = 0;
    for (auto it = range.first; it != range.second; ++it) totalWeight += it->weight;
    if (totalWeight <= 0) return Move();

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> distrib(0, totalWeight - 1);
    int choice = distrib(gen);

    int current = 0;
    for (auto it = range.first; it != range.second; ++it) {
        current += it->weight;
        if (current > choice) {
            Move m = resolvePolyglotMove(it->from, it->to, it->promo);
            return m;
        }
    }
    return Move();
}
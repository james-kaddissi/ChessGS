#include "engine.h"
#include "pst.h"
#include <sstream>
#include <iostream>
#include <vector>

std::vector<Move> moveStack;

ChessEngine::ChessEngine() {
    resetToStartingPosition();
}

ChessEngine::~ChessEngine() {
}

void ChessEngine::resetToStartingPosition() {
    PositionManager::set(DEFAULT_FEN, position);
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
    
    if (position.turn() == WHITE) {
        MoveList<WHITE> list(position);
        for (const Move& move : list) {
            moves.push_back(move);
        }
    } else {
        MoveList<BLACK> list(position);
        for (const Move& move : list) {
            moves.push_back(move);
        }
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
    return true;
}

void ChessEngine::unmakeMove() {
    if (moveStack.empty()) {
        std::cout << "Move stack is empty, nothing to undo.\n";
        return;
    }

    Move lastMove = moveStack.back();
    moveStack.pop_back();

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
    if (side == WHITE) {
        return position.in_check<WHITE>();
    } else {
        return position.in_check<BLACK>();
    }
}

bool ChessEngine::isCheckmate() const {
    if (position.turn() == WHITE) {
        PositionManager tempPosition = position;
        MoveList<WHITE> list(tempPosition);
        return position.in_check<WHITE>() && list.size() == 0;
    } else {
        PositionManager tempPosition = position;
        MoveList<BLACK> list(tempPosition);
        return position.in_check<BLACK>() && list.size() == 0;
    }
}

bool ChessEngine::isStalemate() const {
    if (position.turn() == WHITE) {
        PositionManager tempPosition = position;
        MoveList<WHITE> list(tempPosition);
        return !position.in_check<WHITE>() && list.size() == 0;
    } else {
        PositionManager tempPosition = position;
        MoveList<BLACK> list(tempPosition);
        return !position.in_check<BLACK>() && list.size() == 0;
    }
}

std::string ChessEngine::moveToString(const Move& move) const {
    std::stringstream ss;
    
    Square from = move.from();
    Square to = move.to();
    
    Piece piece = position.at(from);
    PieceType pieceType = piece_type(piece);
    
    if (pieceType != PAWN) {
        ss << "NBRQK"[pieceType - 1];
    }
    
    ss << SQUARE_STR[from] << SQUARE_STR[to];
    
    if (move.flags() == PR_KNIGHT || move.flags() == PC_KNIGHT) {
        ss << "n";
    } else if (move.flags() == PR_BISHOP || move.flags() == PC_BISHOP) {
        ss << "b";
    } else if (move.flags() == PR_ROOK || move.flags() == PC_ROOK) {
        ss << "r";
    } else if (move.flags() == PR_QUEEN || move.flags() == PC_QUEEN) {
        ss << "q";
    }
    
    return ss.str();
} 

/*

    EVALUATION SECTION

*/

/****************************************************************
*  Parameters                                                   *
****************************************************************/
// score bounds
const int MATE_SCORE    = 100000;
const int INF           = 1000000;

// piece values
const int PAWN_VALUE    = 100;
const int KNIGHT_VALUE  = 300;
const int BISHOP_VALUE  = 300;
const int ROOK_VALUE    = 500;
const int QUEEN_VALUE   = 900;

// tempo bonuses
const int MG_TEMPO      = 10;
const int EG_TEMPO      = 5;

// pair bonuses / penalties
const int BISHOP_PAIR   = 30;
const int P_KNIGHT_PAIR = 10;
const int P_ROOK_PAIR   = 20;

// game phase weight
int gamePhase;
const int PHASE_KNIGHT  = 1;
const int PHASE_BISHOP  = 1;
const int PHASE_ROOK    = 2;
const int PHASE_QUEEN   = 4;

inline int flip_rank(int sq) {
    return sq ^ 56; // flips vertical rank (0-63)
}

Bitboard ChessEngine::getFriendlyPieces(Color color) const {
    return (color == WHITE) ? position.all_pieces<WHITE>() : position.all_pieces<BLACK>();
}

int ChessEngine::eval() {
    int gamePhase = game_phase(); // 0 (endgame) to 24 (opening)
    int maxPhase = PHASE_KNIGHT * 4 + PHASE_BISHOP * 4 + PHASE_ROOK * 4 + PHASE_QUEEN * 2; // = 24

    Score white = evaluate_color(WHITE);
    Score black = evaluate_color(BLACK);

    // tempo bonus
    if (position.turn() == WHITE) {
        white.mg += MG_TEMPO;
        white.eg += EG_TEMPO;
    } else {
        black.mg += MG_TEMPO;
        white.eg += EG_TEMPO;
    }

    Score total = white - black;

    // phase interpolation: blend midgame and endgame
    int mgScore = total.mg * gamePhase;
    int egScore = total.eg * (maxPhase - gamePhase);
    int blended = (mgScore + egScore) / maxPhase;

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

    return eval;
}

int ChessEngine::count_material(Color color) {
    int material = 0;

    material += sparse_pop_count(position.bitboard_of(color, PAWN)) * PAWN_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, KNIGHT)) * KNIGHT_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, BISHOP)) * BISHOP_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, ROOK)) * ROOK_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, QUEEN)) * QUEEN_VALUE;

    return material;
}

int ChessEngine::game_phase() {
    int phase = 0;

    phase += sparse_pop_count(position.bitboard_of(WHITE, KNIGHT) | position.bitboard_of(BLACK, KNIGHT)) * PHASE_KNIGHT;
    phase += sparse_pop_count(position.bitboard_of(WHITE, BISHOP) | position.bitboard_of(BLACK, BISHOP)) * PHASE_BISHOP;
    phase += sparse_pop_count(position.bitboard_of(WHITE, ROOK) | position.bitboard_of(BLACK, ROOK)) * PHASE_ROOK;
    phase += sparse_pop_count(position.bitboard_of(WHITE, QUEEN) | position.bitboard_of(BLACK, QUEEN)) * PHASE_QUEEN;

    return phase;
}

Score ChessEngine::evalPawns(Color color) {
    Score score;
    Bitboard pawns = position.bitboard_of(color, PAWN);

    while (pawns) {
        Square sq = pop_lsb(&pawns);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_PAWN_PST[index];
        score.eg += EG_PAWN_PST[index];

        // TODO: add mobility bonus
    }

    return score;
}

Score ChessEngine::evalKnights(Color color) {
    Score score;
    Bitboard knights = position.bitboard_of(color, KNIGHT);

    // pair bonus
    if (sparse_pop_count(knights) > 1) {
        score.mg -= P_KNIGHT_PAIR;
        score.eg -= P_KNIGHT_PAIR;
    }

    int totalMobility = 0;
    int attack = 0;
    while (knights) {
        Square sq = pop_lsb(&knights);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_KNIGHT_PST[index];
        score.eg += EG_KNIGHT_PST[index];

        /****************************************************************
        *  Evaluate mobility. We try to do it in such a way             *
        *  that zero represents average mobility, but our               *
        *  formula of doing so is a puer guess.                         *
        ****************************************************************/
        Bitboard attacks = KNIGHT_ATTACKS[sq];

        // check how many squares are free (not colliding with friendly pieces)
        Bitboard reachable = attacks & ~getFriendlyPieces(color);
        totalMobility += sparse_pop_count(reachable);

        /****************************************************************
        *  Collect data about king attacks                              *
        ****************************************************************/
        Color enemy = (color == WHITE) ? BLACK : WHITE;
        Bitboard enemyKing = position.bitboard_of(enemy, KING);
        Bitboard enemyKingZone = KING_ATTACKS[sq];
        Bitboard attacKing = enemyKingZone & reachable; // hehe get it
        attack += sparse_pop_count(attacKing);
    }

    // mobility bonus
    score.mg += 4 * (totalMobility - 4);
    score.eg += 4 * (totalMobility - 4);

    // attack king bonus
    score.mg += 2 * attack;
    score.eg += 2 * attack;

    return score;
}

Score ChessEngine::evalBishops(Color color) {
    Score score;
    Bitboard bishops = position.bitboard_of(color, BISHOP);

    // pair bonus
    if (sparse_pop_count(bishops) > 1) {
        score.mg += BISHOP_PAIR;
        score.eg += BISHOP_PAIR;
    }

    int totalMobility = 0;
    int attack = 0;
    while (bishops) {
        Square sq = pop_lsb(&bishops);

        int index = (color == WHITE) ? sq : flip_rank(sq);
        
        score.mg += MG_BISHOP_PST[index];
        score.eg += EG_BISHOP_PST[index];

        /****************************************************************
        *  Collect data about mobility                                  *
        ****************************************************************/
        Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
        Bitboard attacks = get_bishop_attacks(sq, occ);

        Bitboard reachable = attacks & ~getFriendlyPieces(color);
        totalMobility += sparse_pop_count(reachable);

        /****************************************************************
        *  Collect data about king attacks                              *
        ****************************************************************/
        Color enemy = (color == WHITE) ? BLACK : WHITE;
        Bitboard enemyKing = position.bitboard_of(enemy, KING);
        Bitboard enemyKingZone = KING_ATTACKS[sq];
        Bitboard attacKing = enemyKingZone & reachable;
        attack += sparse_pop_count(attacKing);
    }
    
    // mobility bonus
    score.mg += 3 * (totalMobility - 7);
    score.eg += 3 * (totalMobility - 7);

    // attack king bonus
    score.mg += 2 * attack;
    score.eg += 2 * attack;

    return score;
}

Score ChessEngine::evalRooks(Color color) {
    Score score;
    Bitboard rooks = position.bitboard_of(color, ROOK);

    // pair bonus
    if (sparse_pop_count(rooks) > 1) {
        score.mg -= P_ROOK_PAIR;
        score.eg -= P_ROOK_PAIR;
    }

    int totalMobility = 0;
    int attack = 0;
    while (rooks) {
        Square sq = pop_lsb(&rooks);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_ROOK_PST[index];
        score.eg += EG_ROOK_PST[index];

        /****************************************************************
        *  Collect data about mobility                                  *
        ****************************************************************/
        Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
        Bitboard attacks = get_rook_attacks(sq, occ);

        Bitboard reachable = attacks & ~getFriendlyPieces(color);
        totalMobility += sparse_pop_count(reachable);

        /****************************************************************
        *  Collect data about king attacks                              *
        ****************************************************************/
        Color enemy = (color == WHITE) ? BLACK : WHITE;
        Bitboard enemyKing = position.bitboard_of(enemy, KING);
        Bitboard enemyKingZone = KING_ATTACKS[sq];
        Bitboard attacKing = enemyKingZone & reachable;
        attack += sparse_pop_count(attacKing);
    }

    // mobility bonus
    score.mg += 2 * (totalMobility - 7);
    score.eg += 4 * (totalMobility - 7);

    // attack king bonus
    score.mg += 3 * attack;
    score.eg += 3 * attack;

    return score;
}

Score ChessEngine::evalQueens(Color color) {
    Score score;
    Bitboard queens = position.bitboard_of(color, QUEEN);

    int totalMobility = 0;
    int attack = 0;
    while (queens) {
        Square sq = pop_lsb(&queens);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_QUEEN_PST[index];
        score.eg += EG_QUEEN_PST[index];

        /****************************************************************
        *  A queen should not be developed too early                    *
        ****************************************************************/
        if (position.turn() == WHITE && rank_of(sq) > RANK2) {
            if (position.at(B1) == WHITE_KNIGHT) score.mg -= 2; score.eg -= 2;
            if (position.at(C1) == WHITE_BISHOP) score.mg -= 2; score.eg -= 2;
            if (position.at(F1) == WHITE_BISHOP) score.mg -= 2; score.eg -= 2;
            if (position.at(G1) == WHITE_KNIGHT) score.mg -= 2; score.eg -= 2;
        }

        if (position.turn() == BLACK && rank_of(sq) < RANK7) {
            if (position.at(B8) == BLACK_KNIGHT) score.mg -= 2; score.eg -= 2;
            if (position.at(C8) == BLACK_BISHOP) score.mg -= 2; score.eg -= 2;
            if (position.at(F8) == BLACK_BISHOP) score.mg -= 2; score.eg -= 2;
            if (position.at(G8) == BLACK_KNIGHT) score.mg -= 2; score.eg -= 2;
        }

        /****************************************************************
        *  Collect data about mobility                                  *
        ****************************************************************/
        Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
        Bitboard attacks = get_rook_attacks(sq, occ) | get_bishop_attacks(sq, occ);

        Bitboard reachable = attacks & ~getFriendlyPieces(color);
        totalMobility += sparse_pop_count(reachable);

        /****************************************************************
        *  Collect data about king attacks                              *
        ****************************************************************/
        Color enemy = (color == WHITE) ? BLACK : WHITE;
        Bitboard enemyKing = position.bitboard_of(enemy, KING);
        Bitboard enemyKingZone = KING_ATTACKS[sq];
        Bitboard attacKing = enemyKingZone & reachable;
        attack += sparse_pop_count(attacKing);
    }

    // mobility bonus
    score.mg += 1 * (totalMobility - 14);
    score.mg += 2 * (totalMobility - 14);

    // attack king bonus
    score.mg += 4 * attack;
    score.eg += 4 * attack;

    return score;
}

int ChessEngine::search(int depth, int alpha, int beta) {
    if (depth == 0) {
        return eval();
    }

    std::vector<Move> moves = generateLegalMoves();
    if (moves.size() == 0) {
        if (isInCheck(position.turn())) {
            return -MATE_SCORE + depth; // checkmate
        }
        return 0; // stalemate
    }

    for (auto move : moves) {
        makeMove(move);
        int evaluation = -search(depth - 1, -beta, -alpha);
        unmakeMove();
        if (evaluation >= beta) {
            return beta; // prune branch
        }
        alpha = std::max(alpha, evaluation);
    }

    return alpha;
}

Move ChessEngine::getBestMove(int depth) {
    PositionManager originalPosition = position;

    int alpha = -INF;
    int beta = INF;
    Move bestMove = NULL;
    int bestEval = -INF;

    std::vector<Move> moves = generateLegalMoves();
    for (const auto& m : moves) {
        std::cout << moveToString(m) << "\n";
    }

    for (auto move : moves) {
        makeMove(move);
        int evaluation = -search(depth - 1, -beta, -alpha);
        unmakeMove();

        if (evaluation > bestEval) {
            bestEval = evaluation;
            bestMove = move;
        }

        alpha = std::max(alpha, evaluation);
    }

    position = originalPosition;

    if (bestMove == NULL) {
        std::cerr << "WARNING: No valid best move found at depth " << depth << "\n";
    }

    std::cout << "Best move: " << moveToString(bestMove) << " with evaluation " << bestEval << "\n";

    return bestMove;
}

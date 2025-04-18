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

const int MATE_SCORE    = 100000;
const int INF           = 1000000;

const int PAWN_VALUE    = 100;
const int KNIGHT_VALUE  = 300;
const int BISHOP_VALUE  = 300;
const int ROOK_VALUE    = 500;
const int QUEEN_VALUE   = 900;

const int BISHOP_PAIR   = 30;
const int P_KNIGHT_PAIR = 10;
const int P_ROOK_PAIR   = 20;

inline int flip_rank(int sq) {
    return sq ^ 56; // flips vertical rank (0-63)
}

Bitboard ChessEngine::getFriendlyPieces(Color color) const {
    return (color == WHITE) ? position.all_pieces<WHITE>() : position.all_pieces<BLACK>();
}

int ChessEngine::eval() {
  int whiteEval = count_material(WHITE) + evalPawns(WHITE) + evalKnights(WHITE) + evalBishops(WHITE) + evalRooks(WHITE) + evalQueens(WHITE);
  int blackEval = count_material(BLACK) + evalPawns(BLACK) + evalKnights(BLACK) + evalBishops(BLACK) + evalRooks(BLACK) + evalQueens(BLACK);

  int evaluation = whiteEval - blackEval;

  int perspective = position.turn() == WHITE ? 1 : -1;
  return evaluation * perspective;
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

int ChessEngine::evalPawns(Color color) {
    Bitboard pawns = position.bitboard_of(color, PAWN);
    int evaluation = 0;

    while (pawns) {
        Square sq = pop_lsb(&pawns);

        int index = (color == WHITE) ? sq : flip_rank(sq);
        evaluation += PAWN_PST[index];

        // TODO: add mobility bonus
    }

    return evaluation;
}

int ChessEngine::evalKnights(Color color) {
    Bitboard knights = position.bitboard_of(color, KNIGHT);
    int evaluation = 0;

    // pair bonus
    if (sparse_pop_count(knights) > 1) {
        evaluation -= P_KNIGHT_PAIR;
    }

    while (knights) {
        Square sq = pop_lsb(&knights);

        int index = (color == WHITE) ? sq : flip_rank(sq);
        evaluation += KNIGHT_PST[index];

        /****************************************************************
        *  Evaluate mobility. We try to do it in such a way             *
        *  that zero represents average mobility, but our               *
        *  formula of doing so is a puer guess.                         *
        ****************************************************************/
        Bitboard attack = KNIGHT_ATTACKS[sq];

        // check how many squares are free (not colliding with friendly pieces)
        int mobility = sparse_pop_count(attack & getFriendlyPieces(color));

        evaluation = 4 * (mobility - 4);
    }

    return evaluation;
}

int ChessEngine::evalBishops(Color color) {
    Bitboard bishops = position.bitboard_of(color, BISHOP);

    int evaluation = 0;

    // pair bonus
    if (sparse_pop_count(bishops) > 1) {
        evaluation += BISHOP_PAIR;
    }

    while (bishops) {
        Square sq = pop_lsb(&bishops);

        int index = (color == WHITE) ? sq : flip_rank(sq);
        evaluation += BISHOP_PST[index];

        /****************************************************************
        *  Collect data about mobility                                  *
        ****************************************************************/
        Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
        Bitboard attack = get_bishop_attacks(sq, occ);

        int mobility = sparse_pop_count(attack & ~getFriendlyPieces(color));
        evaluation += 3 * (mobility - 7);
    }

    return evaluation;
}

int ChessEngine::evalRooks(Color color) {
    Bitboard rooks = position.bitboard_of(color, ROOK);

    int evaluation = 0;

    // pair bonus
    if (sparse_pop_count(rooks) > 1) {
        evaluation -= P_ROOK_PAIR;
    }

    while (rooks) {
        Square sq = pop_lsb(&rooks);

        int index = (color == WHITE) ? sq : flip_rank(sq);
        evaluation += ROOK_PST[index];

        /****************************************************************
        *  Collect data about mobility                                  *
        ****************************************************************/
        Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
        Bitboard attack = get_rook_attacks(sq, occ);

        int mobility = sparse_pop_count(attack & ~getFriendlyPieces(color));
        evaluation += 3 * (mobility - 7);
    }

    return evaluation;
}

int ChessEngine::evalQueens(Color color) {
    Bitboard queens = position.bitboard_of(color, QUEEN);

    int evaluation = 0;

    while (queens) {
        Square sq = pop_lsb(&queens);

        int index = (color == WHITE) ? sq : flip_rank(sq);
        evaluation += BISHOP_PST[index];

        /****************************************************************
        *  Collect data about mobility                                  *
        ****************************************************************/
        Bitboard occ = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
        Bitboard attack = get_rook_attacks(sq, occ) | get_bishop_attacks(sq, occ);

        int mobility = sparse_pop_count(attack & ~getFriendlyPieces(color));
        evaluation += 2 * (mobility - 14);
    }

    return evaluation;
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

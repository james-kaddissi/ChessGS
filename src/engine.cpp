#include "engine.h"
#include <sstream>
#include <iostream>

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
    if (position.turn() == WHITE) {
        position.play<WHITE>(move);
    } else {
        position.play<BLACK>(move);
    }
    return true;
}

void ChessEngine::unmakeMove() {
    if (position.ply() <= 0) {
        return;
    }
    
    Bitboard lastMoveEntry = position.history[position.ply()].entry;
    if (lastMoveEntry == 0) {
        return;
    }
    
    Square to = bsf(lastMoveEntry);
    lastMoveEntry &= ~(1ULL << to);
    Square from = bsf(lastMoveEntry);
    
    Move lastMove(from, to);
    
    if (position.turn() == WHITE) {
        position.undo<BLACK>(lastMove);
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

const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 300;
const int BISHOP_VALUE = 300;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;

int ChessEngine::eval() {
  int whiteEval = count_material(WHITE);
  int blackEval = count_material(BLACK);

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

int ChessEngine::search(int depth, int alpha, int beta) {
  if (depth == 0) {
    return eval();
  }

  std::vector<Move> moves = generateLegalMoves();
  if (moves.size() == 0) {
    if (isInCheck(position.turn())) {
        return std::numeric_limits<int>::min(); // checkmate
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
    int alpha = std::numeric_limits<int>::min();
    int beta = std::numeric_limits<int>::max();
    Move bestMove = NULL;
    int bestEval = std::numeric_limits<int>::min();

    std::vector<Move> moves = generateLegalMoves();
    std::cout << "Legal moves at depth " << depth << ":\n";
    for (const auto& m : moves) {
        std::cout << moveToString(m) << "\n";
    }

    for (auto move : moves) {
        makeMove(move);
        int evaluation = -search(depth - 1, -beta, -alpha);
        unmakeMove();

        std::cout << "Move " << moveToString(move) << " has evaluation " << evaluation << "\n";

        if (evaluation > bestEval) {
            bestEval = evaluation;
            bestMove = move;
        }

        alpha = std::max(alpha, evaluation);
    }

    if (bestMove == NULL) {
        std::cerr << "WARNING: No valid best move found at depth " << depth << "\n";
    }

    std::cout << "Best move: " << moveToString(bestMove) << " with evaluation " << bestEval << "\n";

    return bestMove;
}

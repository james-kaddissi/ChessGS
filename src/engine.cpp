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
#include "engine.h"
#include <iostream>
#include <algorithm>
#include <sstream>

void ChessEngine::initialize() {
    initAttackTables();
    initMagicBitboards();
    initBetweenAndLineSquares();
}

ChessEngine::ChessEngine() {
    resetToStartingPosition();
}

ChessEngine::~ChessEngine() {
}

void ChessEngine::resetToStartingPosition() {
    for (int c = 0; c < 2; ++c) {
        for (int p = 0; p < 6; ++p) {
            pieces[c][p] = 0ULL;
        }
        colorBitboards[c] = 0ULL;
    }
    occupiedSquares = 0ULL;

    for (int p = 0; p < 6; ++p) {
        pieces[WHITE][p] = startingPieces[WHITE][p];
        pieces[BLACK][p] = startingPieces[BLACK][p];
    }

    colorBitboards[WHITE] = 0ULL;
    colorBitboards[BLACK] = 0ULL;

    for (int p = 0; p < 6; ++p) {
        colorBitboards[WHITE] |= pieces[WHITE][p];
        colorBitboards[BLACK] |= pieces[BLACK][p];
    }

    occupiedSquares = colorBitboards[WHITE] | colorBitboards[BLACK];

    sideToMove = WHITE;
    enPassantSquare = NO_SQ;
    castlingRights = WHITE_KINGSIDE | WHITE_QUEENSIDE | BLACK_KINGSIDE | BLACK_QUEENSIDE;
    halfMoveClock = 0;
    fullMoveNumber = 1;
    historyIndex = 0;
}

void ChessEngine::addPiece(Square sq, PieceType pt, Color c) {
    Bitboard bb = squareToBitboard(sq);
    pieces[c][pt] |= bb;
    colorBitboards[c] |= bb;
    occupiedSquares |= bb;
}

void ChessEngine::removePiece(Square sq, PieceType pt, Color c) {
    Bitboard bb = squareToBitboard(sq);
    pieces[c][pt] &= ~bb;
    colorBitboards[c] &= ~bb;
    occupiedSquares &= ~bb;
}

void ChessEngine::movePiece(Square from, Square to, PieceType pt, Color c) {
    Bitboard fromBB = squareToBitboard(from);
    Bitboard toBB = squareToBitboard(to);
    Bitboard fromTo = fromBB ^ toBB;
    
    pieces[c][pt] ^= fromTo;
    colorBitboards[c] ^= fromTo;
    occupiedSquares ^= fromTo;
}

Bitboard ChessEngine::getPieces(Color color, PieceType type) const {
    return pieces[color][type];
}

Bitboard ChessEngine::getColorPieces(Color color) const {
    return colorBitboards[color];
}

Bitboard ChessEngine::getOccupiedSquares() const {
    return occupiedSquares;
}

PieceType ChessEngine::getPieceAt(Square sq, Color &outColor) const {
    Bitboard sqBitboard = squareToBitboard(sq);

    if (!(occupiedSquares & sqBitboard)) {
        outColor = WHITE;
        return NONE;
    }

    if (colorBitboards[WHITE] & sqBitboard) {
        outColor = WHITE;
    } else {
        outColor = BLACK;
    }

    for (int p = 0; p < 6; ++p) {
        if (pieces[outColor][p] & sqBitboard) {
            return static_cast<PieceType>(p);
        }
    }

    return NONE;
}

bool ChessEngine::isSquare(Square square) const {
    return square >= A1 && square <= H8; 
}

bool ChessEngine::isSquareAttacked(Square square, Color attacker) const {
    if (PawnAttacks[attacker ^ 1][square] & pieces[attacker][PAWN])
        return true;
        
    if (KnightAttacks[square] & pieces[attacker][KNIGHT])
        return true;
        
    if (KingAttacks[square] & pieces[attacker][KING])
        return true;
        
    if (getBishopAttacks(square, occupiedSquares) & 
        (pieces[attacker][BISHOP] | pieces[attacker][QUEEN]))
        return true;
        
    if (getRookAttacks(square, occupiedSquares) & 
        (pieces[attacker][ROOK] | pieces[attacker][QUEEN]))
        return true;
        
    return false;
}

bool ChessEngine::isInCheck(Color color) const {
    Square kingSquare = lsb(pieces[color][KING]);
    return isSquareAttacked(kingSquare, static_cast<Color>(color ^ 1));
}

Bitboard ChessEngine::getPinnedPieces(Color kingColor) const {
    Bitboard pinned = 0;
    Square kingSquare = lsb(pieces[kingColor][KING]);
    Color opponent = static_cast<Color>(kingColor ^ 1);
    Bitboard ourPieces = colorBitboards[kingColor];
    
    // Check for bishop/queen pins
    Bitboard bishopQueens = pieces[opponent][BISHOP] | pieces[opponent][QUEEN];
    Bitboard potentialPinners = getBishopAttacks(kingSquare, 0) & bishopQueens;
    
    while (potentialPinners) {
        Square pinnerSq = lsb(potentialPinners);
        potentialPinners &= potentialPinners - 1;
        
        Bitboard between = BetweenSquares[kingSquare][pinnerSq] & occupiedSquares;
        
        if (popCount(between) == 1 && (between & ourPieces)) {
            pinned |= between;
        }
    }
    
    // rook/queen pins
    Bitboard rookQueens = pieces[opponent][ROOK] | pieces[opponent][QUEEN];
    potentialPinners = getRookAttacks(kingSquare, 0) & rookQueens;
    
    while (potentialPinners) {
        Square pinnerSq = lsb(potentialPinners);
        potentialPinners &= potentialPinners - 1;
        
        Bitboard between = BetweenSquares[kingSquare][pinnerSq] & occupiedSquares;
        
        if (popCount(between) == 1 && (between & ourPieces)) {
            pinned |= between;
        }
    }
    
    return pinned;
}

bool ChessEngine::makeMove(Move move) {
    Square from = getFrom(move);
    Square to = getTo(move);
    MoveFlag flag = getFlag(move);
    
    Color pieceColor;
    PieceType pieceType = getPieceAt(from, pieceColor);
    
    if (pieceType == NONE || pieceColor != sideToMove) {
        return false;
    }
    
    // save undo move
    UndoInfo &undo = history[historyIndex++];
    undo.move = move;
    undo.castlingRights = castlingRights;
    undo.enPassantSquare = enPassantSquare;
    undo.halfMoveClock = halfMoveClock;
    undo.isNullMove = false;
    
    Square oldEnPassant = enPassantSquare;
    enPassantSquare = NO_SQ;
    
    halfMoveClock++;
    
    if (isCapture(move)) {
        halfMoveClock = 0;
        Square captureSquare = to;
        
        if (flag == EP_CAPTURE) {
            captureSquare = static_cast<Square>(to + (sideToMove == WHITE ? -8 : 8));
        }
        
        Color capturedPieceColor;
        undo.capturedPiece = getPieceAt(captureSquare, capturedPieceColor);
        
        if (undo.capturedPiece != NONE) {
            removePiece(captureSquare, undo.capturedPiece, capturedPieceColor);
        }
    } else {
        undo.capturedPiece = NONE;
    }
    
    if (flag == KING_CASTLE || flag == QUEEN_CASTLE) { // handle castling
        removePiece(from, KING, sideToMove);
        addPiece(to, KING, sideToMove);
        
        Square rookFrom, rookTo;
        if (flag == KING_CASTLE) {
            rookFrom = static_cast<Square>(from + 3);
            rookTo = static_cast<Square>(from + 1);
        } else {
            rookFrom = static_cast<Square>(from - 4);
            rookTo = static_cast<Square>(from - 1);
        }
        
        removePiece(rookFrom, ROOK, sideToMove);
        addPiece(rookTo, ROOK, sideToMove);
    } else if (flag >= PROMOTION_KNIGHT && flag <= PROMOTION_QUEEN_CAPTURE) { // handle promotion
        PieceType promotionType = getPromotionType(move);
        removePiece(from, PAWN, sideToMove);
        addPiece(to, promotionType, sideToMove);
    } else {
        movePiece(from, to, pieceType, sideToMove);
    }
    
    if (pieceType == PAWN) {
        halfMoveClock = 0;
        
        if (flag == DOUBLE_PAWN_PUSH) {
            enPassantSquare = static_cast<Square>((from + to) / 2);
        }
    }
    
    if (pieceType == KING) {
        if (sideToMove == WHITE) {
            castlingRights &= ~(WHITE_KINGSIDE | WHITE_QUEENSIDE);
        } else {
            castlingRights &= ~(BLACK_KINGSIDE | BLACK_QUEENSIDE);
        }
    } else if (pieceType == ROOK) {
        if (sideToMove == WHITE) {
            if (from == A1) castlingRights &= ~WHITE_QUEENSIDE;
            if (from == H1) castlingRights &= ~WHITE_KINGSIDE;
        } else {
            if (from == A8) castlingRights &= ~BLACK_QUEENSIDE;
            if (from == H8) castlingRights &= ~BLACK_KINGSIDE;
        }
    }
    
    if (undo.capturedPiece == ROOK) {
        if (to == A1) castlingRights &= ~WHITE_QUEENSIDE;
        if (to == H1) castlingRights &= ~WHITE_KINGSIDE;
        if (to == A8) castlingRights &= ~BLACK_QUEENSIDE;
        if (to == H8) castlingRights &= ~BLACK_KINGSIDE;
    }
    
    // change side
    sideToMove = static_cast<Color>(sideToMove ^ 1);
    if (sideToMove == WHITE) {
        fullMoveNumber++;
    }
    
    return true;
}

void ChessEngine::unmakeMove() {
    if (historyIndex == 0) return;
    
    UndoInfo &undo = history[--historyIndex];
    Move move = undo.move;
    
    if (undo.isNullMove) {
        sideToMove = static_cast<Color>(sideToMove ^ 1);
        enPassantSquare = undo.enPassantSquare;
        return;
    }
    
    sideToMove = static_cast<Color>(sideToMove ^ 1);
    
    Square from = getFrom(move);
    Square to = getTo(move);
    MoveFlag flag = getFlag(move);
    
    if (flag == KING_CASTLE || flag == QUEEN_CASTLE) {
        // for undoing castling
        removePiece(to, KING, sideToMove);
        addPiece(from, KING, sideToMove);
        
        Square rookFrom, rookTo;
        if (flag == KING_CASTLE) {
            rookFrom = static_cast<Square>(from + 3);
            rookTo = static_cast<Square>(from + 1);
        } else {
            rookFrom = static_cast<Square>(from - 4);
            rookTo = static_cast<Square>(from - 1);
        }
        
        removePiece(rookTo, ROOK, sideToMove);
        addPiece(rookFrom, ROOK, sideToMove);
    } else if (flag >= PROMOTION_KNIGHT && flag <= PROMOTION_QUEEN_CAPTURE) {
        // for undoing promotion
        removePiece(to, getPromotionType(move), sideToMove);
        addPiece(from, PAWN, sideToMove);
    } else {
        Color pieceColor;
        PieceType pieceType = getPieceAt(to, pieceColor);
        
        movePiece(to, from, pieceType, sideToMove);
    }
    
    if (isCapture(move)) {
        Square captureSquare = to;
        
        if (flag == EP_CAPTURE) {
            captureSquare = static_cast<Square>(to + (sideToMove == WHITE ? -8 : 8));
        }
        
        if (undo.capturedPiece != NONE) {
            addPiece(captureSquare, undo.capturedPiece, static_cast<Color>(sideToMove ^ 1));
        }
    }
    
    // restore old state
    castlingRights = undo.castlingRights;
    enPassantSquare = undo.enPassantSquare;
    halfMoveClock = undo.halfMoveClock;
    
    if (sideToMove == BLACK) {
        fullMoveNumber--;
    }
}

bool ChessEngine::isLegalMove(Move move) const {
    Square from = getFrom(move);
    Square to = getTo(move);
    
    Color pieceColor;
    PieceType pieceType = getPieceAt(from, pieceColor);
    
    if (pieceType == NONE || pieceColor != sideToMove) {
        return false;
    }
    
    // cant move into check
    if (pieceType == KING) {
        if (isSquareAttacked(to, static_cast<Color>(sideToMove ^ 1))) {
            return false;
        }

        // castling
        MoveFlag flag = getFlag(move);
        if (flag == KING_CASTLE || flag == QUEEN_CASTLE) {
            if (isInCheck(sideToMove)) {
                return false;
            }
            
            Square middle;
            if (flag == KING_CASTLE) {
                middle = static_cast<Square>(from + 1);
            } else {
                middle = static_cast<Square>(from - 1);
            }
            
            if (isSquareAttacked(middle, static_cast<Color>(sideToMove ^ 1))) {
                return false;
            }
        }
        
        return true;
    }
    
    // pins
    Square kingSquare = lsb(pieces[sideToMove][KING]);
    Bitboard pinnedPieces = getPinnedPieces(sideToMove);
    
    // moving pinned piece must stay pinned
    if (pinnedPieces & squareToBitboard(from)) {
        if (!(LineSquares[kingSquare][from] & squareToBitboard(to))) {
            return false;
        }
    }
    
    return true;
}

std::vector<Move> ChessEngine::generateLegalMoves() const {
    std::vector<Move> pseudoLegal = generatePseudoLegalMoves();
    std::vector<Move> legal;
    legal.reserve(pseudoLegal.size());
    
    for (Move move : pseudoLegal) {
        if (isLegalMove(move)) {
            legal.push_back(move);
        }
    }
    
    return legal;
}

std::vector<Move> ChessEngine::generatePseudoLegalMoves() const {
    std::vector<Move> captures = generateCapturesAndPromotions();
    std::vector<Move> quiets = generateQuietMoves();
    
    captures.insert(captures.end(), quiets.begin(), quiets.end());
    return captures;
}

std::vector<Move> ChessEngine::generateCapturesAndPromotions() const {
    std::vector<Move> moves;
    moves.reserve(50);
    
    Color us = sideToMove;
    Color them = static_cast<Color>(us ^ 1);
    Bitboard ourPieces = colorBitboards[us];
    Bitboard theirPieces = colorBitboards[them];
    
    Bitboard pawns = pieces[us][PAWN];
    Bitboard promotionRank = (us == WHITE) ? RANK_7 : RANK_2;
    Bitboard nonPromotionPawns = pawns & ~promotionRank;
    Bitboard promotionPawns = pawns & promotionRank;
    
    if (nonPromotionPawns) {
        Bitboard left = (us == WHITE) 
            ? shiftNorthWest(nonPromotionPawns) & theirPieces
            : shiftSouthWest(nonPromotionPawns) & theirPieces;
            
        Bitboard right = (us == WHITE)
            ? shiftNorthEast(nonPromotionPawns) & theirPieces
            : shiftSouthEast(nonPromotionPawns) & theirPieces;
            
        while (left) {
            Square to = lsb(left);
            left &= left - 1;
            
            Square from = (us == WHITE) ? static_cast<Square>(to - 7) : static_cast<Square>(to + 9);
            moves.push_back(createMove(from, to, CAPTURE));
        }
        
        while (right) {
            Square to = lsb(right);
            right &= right - 1;
            
            Square from = (us == WHITE) ? static_cast<Square>(to - 9) : static_cast<Square>(to + 7);
            moves.push_back(createMove(from, to, CAPTURE));
        }
    }
    
    if (promotionPawns) {
        Bitboard left = (us == WHITE)
            ? shiftNorthWest(promotionPawns) & theirPieces
            : shiftSouthWest(promotionPawns) & theirPieces;
            
        Bitboard right = (us == WHITE)
            ? shiftNorthEast(promotionPawns) & theirPieces
            : shiftSouthEast(promotionPawns) & theirPieces;
            
        while (left) {
            Square to = lsb(left);
            left &= left - 1;
            
            Square from = (us == WHITE) ? static_cast<Square>(to - 7) : static_cast<Square>(to + 9);
            moves.push_back(createMove(from, to, PROMOTION_QUEEN_CAPTURE));
            moves.push_back(createMove(from, to, PROMOTION_ROOK_CAPTURE));
            moves.push_back(createMove(from, to, PROMOTION_BISHOP_CAPTURE));
            moves.push_back(createMove(from, to, PROMOTION_KNIGHT_CAPTURE));
        }
        
        while (right) {
            Square to = lsb(right);
            right &= right - 1;
            
            Square from = (us == WHITE) ? static_cast<Square>(to - 9) : static_cast<Square>(to + 7);
            moves.push_back(createMove(from, to, PROMOTION_QUEEN_CAPTURE));
            moves.push_back(createMove(from, to, PROMOTION_ROOK_CAPTURE));
            moves.push_back(createMove(from, to, PROMOTION_BISHOP_CAPTURE));
            moves.push_back(createMove(from, to, PROMOTION_KNIGHT_CAPTURE));
        }
        
        Bitboard advance = (us == WHITE)
            ? shiftNorth(promotionPawns) & ~occupiedSquares
            : shiftSouth(promotionPawns) & ~occupiedSquares;
            
        while (advance) {
            Square to = lsb(advance);
            advance &= advance - 1;
            
            Square from = (us == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
            moves.push_back(createMove(from, to, PROMOTION_QUEEN));
            moves.push_back(createMove(from, to, PROMOTION_ROOK));
            moves.push_back(createMove(from, to, PROMOTION_BISHOP));
            moves.push_back(createMove(from, to, PROMOTION_KNIGHT));
        }
    }
    
    if (enPassantSquare != NO_SQ) {
        Bitboard epPawns = PawnAttacks[them][enPassantSquare] & pieces[us][PAWN];
        
        while (epPawns) {
            Square from = lsb(epPawns);
            epPawns &= epPawns - 1;
            
            moves.push_back(createMove(from, enPassantSquare, EP_CAPTURE));
        }
    }
    
    Bitboard knights = pieces[us][KNIGHT];
    while (knights) {
        Square from = lsb(knights);
        knights &= knights - 1;
        
        Bitboard attacks = KnightAttacks[from] & theirPieces;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, CAPTURE));
        }
    }
    
    Bitboard bishops = pieces[us][BISHOP];
    while (bishops) {
        Square from = lsb(bishops);
        bishops &= bishops - 1;
        
        Bitboard attacks = getBishopAttacks(from, occupiedSquares) & theirPieces;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, CAPTURE));
        }
    }
    
    Bitboard rooks = pieces[us][ROOK];
    while (rooks) {
        Square from = lsb(rooks);
        rooks &= rooks - 1;
        
        Bitboard attacks = getRookAttacks(from, occupiedSquares) & theirPieces;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, CAPTURE));
        }
    }
    
    Bitboard queens = pieces[us][QUEEN];
    while (queens) {
        Square from = lsb(queens);
        queens &= queens - 1;
        
        Bitboard attacks = getQueenAttacks(from, occupiedSquares) & theirPieces;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, CAPTURE));
        }
    }
    
    Square kingSquare = lsb(pieces[us][KING]);
    Bitboard kingAttacks = KingAttacks[kingSquare] & theirPieces;
    
    while (kingAttacks) {
        Square to = lsb(kingAttacks);
        kingAttacks &= kingAttacks - 1;
        
        moves.push_back(createMove(kingSquare, to, CAPTURE));
    }
    
    return moves;
}

std::vector<Move> ChessEngine::generateQuietMoves() const {
    std::vector<Move> moves;
    moves.reserve(100);
    
    Color us = sideToMove;
    Bitboard ourPieces = colorBitboards[us];
    Bitboard emptySquares = ~occupiedSquares;
    
    Bitboard pawns = pieces[us][PAWN];
    Bitboard promotionRank = (us == WHITE) ? RANK_7 : RANK_2;
    Bitboard nonPromotionPawns = pawns & ~promotionRank;
    
    Bitboard singlePush = (us == WHITE)
        ? shiftNorth(nonPromotionPawns) & emptySquares
        : shiftSouth(nonPromotionPawns) & emptySquares;
        
    Bitboard doublePushCandidates = (us == WHITE)
        ? singlePush & RANK_3
        : singlePush & RANK_6;
        
    Bitboard doublePush = (us == WHITE)
        ? shiftNorth(doublePushCandidates) & emptySquares
        : shiftSouth(doublePushCandidates) & emptySquares;
        
    while (singlePush) {
        Square to = lsb(singlePush);
        singlePush &= singlePush - 1;
        
        Square from = (us == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
        moves.push_back(createMove(from, to, QUIET));
    }
    
    while (doublePush) {
        Square to = lsb(doublePush);
        doublePush &= doublePush - 1;
        
        Square from = (us == WHITE) ? static_cast<Square>(to - 16) : static_cast<Square>(to + 16);
        moves.push_back(createMove(from, to, DOUBLE_PAWN_PUSH));
    }
    
    Bitboard knights = pieces[us][KNIGHT];
    while (knights) {
        Square from = lsb(knights);
        knights &= knights - 1;
        
        Bitboard attacks = KnightAttacks[from] & emptySquares;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, QUIET));
        }
    }
    
    Bitboard bishops = pieces[us][BISHOP];
    while (bishops) {
        Square from = lsb(bishops);
        bishops &= bishops - 1;
        
        Bitboard attacks = getBishopAttacks(from, occupiedSquares) & emptySquares;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, QUIET));
        }
    }
    
    Bitboard rooks = pieces[us][ROOK];
    while (rooks) {
        Square from = lsb(rooks);
        rooks &= rooks - 1;
        
        Bitboard attacks = getRookAttacks(from, occupiedSquares) & emptySquares;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, QUIET));
        }
    }
    
    Bitboard queens = pieces[us][QUEEN];
    while (queens) {
        Square from = lsb(queens);
        queens &= queens - 1;
        
        Bitboard attacks = getQueenAttacks(from, occupiedSquares) & emptySquares;
        
        while (attacks) {
            Square to = lsb(attacks);
            attacks &= attacks - 1;
            
            moves.push_back(createMove(from, to, QUIET));
        }
    }
    
    Square kingSquare = lsb(pieces[us][KING]);
    Bitboard kingAttacks = KingAttacks[kingSquare] & emptySquares;
    
    while (kingAttacks) {
        Square to = lsb(kingAttacks);
        kingAttacks &= kingAttacks - 1;
        
        moves.push_back(createMove(kingSquare, to, QUIET));
    }
    
    if (!isInCheck(us)) {
        if (us == WHITE) {
            if ((castlingRights & WHITE_KINGSIDE) &&
                !(occupiedSquares & (squareToBitboard(F1) | squareToBitboard(G1))) &&
                !isSquareAttacked(F1, BLACK) && !isSquareAttacked(G1, BLACK)) {
                moves.push_back(createMove(E1, G1, KING_CASTLE));
            }
            
            if ((castlingRights & WHITE_QUEENSIDE) &&
                !(occupiedSquares & (squareToBitboard(D1) | squareToBitboard(C1) | squareToBitboard(B1))) &&
                !isSquareAttacked(D1, BLACK) && !isSquareAttacked(C1, BLACK)) {
                moves.push_back(createMove(E1, C1, QUEEN_CASTLE));
            }
        } else {
            if ((castlingRights & BLACK_KINGSIDE) &&
                !(occupiedSquares & (squareToBitboard(F8) | squareToBitboard(G8))) &&
                !isSquareAttacked(F8, WHITE) && !isSquareAttacked(G8, WHITE)) {
                moves.push_back(createMove(E8, G8, KING_CASTLE));
            }
            
            if ((castlingRights & BLACK_QUEENSIDE) &&
                !(occupiedSquares & (squareToBitboard(D8) | squareToBitboard(C8) | squareToBitboard(B8))) &&
                !isSquareAttacked(D8, WHITE) && !isSquareAttacked(C8, WHITE)) {
                moves.push_back(createMove(E8, C8, QUEEN_CASTLE));
            }
        }
    }
    
    return moves;
}

void ChessEngine::printBoard() const {
    std::cout << "  a b c d e f g h" << std::endl;
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << " ";
        for (int file = 0; file < 8; ++file) {
            Square sq = static_cast<Square>(rank * 8 + file);
            Color pieceColor;
            PieceType pieceType = getPieceAt(sq, pieceColor);

            char pieceChar = '.';
            if (pieceType != NONE) {
                const char *pieces = "pnbrqk";
                pieceChar = pieces[pieceType];
                if (pieceColor == WHITE) {
                    pieceChar = std::toupper(pieceChar);
                }
            }
            std::cout << pieceChar << " ";
        }
        std::cout << rank + 1 << std::endl;
    }
    std::cout << "  a b c d e f g h" << std::endl;

    std::cout << "Side to move: " << (sideToMove == WHITE ? "White" : "Black") << std::endl;
    std::cout << "Castling rights: ";
    if (castlingRights & WHITE_KINGSIDE) std::cout << "K";
    if (castlingRights & WHITE_QUEENSIDE) std::cout << "Q";
    if (castlingRights & BLACK_KINGSIDE) std::cout << "k";
    if (castlingRights & BLACK_QUEENSIDE) std::cout << "q";
    if (castlingRights == 0) std::cout << "-";
    std::cout << std::endl;
    
    if (enPassantSquare != NO_SQ) {
        std::cout << "En passant: " << squareToString(enPassantSquare) << std::endl;
    }
    
    std::cout << "Halfmove clock: " << halfMoveClock << std::endl;
    std::cout << "Fullmove number: " << fullMoveNumber << std::endl;
}

std::string ChessEngine::squareToString(Square square) const {
    if (square == NO_SQ) return "-";
    char file = 'a' + getFile(square);
    char rank = '1' + getRank(square);
    return std::string(1, file) + std::string(1, rank);
}

std::string ChessEngine::moveToString(Move move) const {
    std::string result = squareToString(getFrom(move)) + squareToString(getTo(move));
    
    MoveFlag flag = getFlag(move);
    if (flag >= PROMOTION_KNIGHT && flag <= PROMOTION_QUEEN_CAPTURE) {
        char promotionChar = ' ';
        switch (getPromotionType(move)) {
            case KNIGHT: promotionChar = 'n'; break;
            case BISHOP: promotionChar = 'b'; break;
            case ROOK: promotionChar = 'r'; break;
            case QUEEN: promotionChar = 'q'; break;
            default: break;
        }
        result += promotionChar;
    }
    
    return result;
}

Move ChessEngine::parseMove(const std::string& moveStr) const {
    if (moveStr.length() < 4) return 0;
    
    int fromFile = moveStr[0] - 'a';
    int fromRank = moveStr[1] - '1';
    int toFile = moveStr[2] - 'a';
    int toRank = moveStr[3] - '1';
    
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7) {
        return 0;
    }
    
    Square from = static_cast<Square>(fromRank * 8 + fromFile);
    Square to = static_cast<Square>(toRank * 8 + toFile);
    
    // check move
    std::vector<Move> legalMoves = generateLegalMoves();
    
    PieceType promotionType = NONE;
    if (moveStr.length() > 4) {
        switch (moveStr[4]) {
            case 'n': promotionType = KNIGHT; break;
            case 'b': promotionType = BISHOP; break;
            case 'r': promotionType = ROOK; break;
            case 'q': promotionType = QUEEN; break;
            default: break;
        }
    }
    
    for (Move move : legalMoves) {
        if (getFrom(move) == from && getTo(move) == to) {
            if (isPromotion(move)) {
                if (getPromotionType(move) == promotionType) {
                    return move;
                }
            } else {
                return move;
            }
        }
    }
    
    return 0;
}
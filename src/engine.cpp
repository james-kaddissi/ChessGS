#include "engine.h"
#include "pst.h"
#include <sstream>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <SDL2/SDL.h>

std::vector<Move> moveStack;

ChessEngine::ChessEngine()
{
    resetToStartingPosition();
    current_ply = 0;
    last_score = 0;
    total_nodes = 0;
    last_search_depth = 0;
    start_time = 0;
    allocated_time_ms = 0;
    time_up_flag = false;

    clearTables();

    try {
        if (std::ifstream("book.bin").good()) {
            loadOpeningBook("book.bin");
        }
        else {
            std::cerr << "Warning: Opening book file not found." << std::endl;
            openingBook.clear();
        }
    } catch (...) {
        std::cerr << "Warning: Error loading opening book." << std::endl;
        openingBook.clear();
    }
}

ChessEngine::~ChessEngine()
{
}

void ChessEngine::resetToStartingPosition()
{
    PositionManager::set(DEFAULT_FEN, position);
}

PieceType ChessEngine::getPieceAt(Square sq, Color &color)
{
    Piece piece = position.at(sq);
    if (piece == NO_PIECE)
    {
        color = WHITE;
        return NONE;
    }

    color = piece_color(piece);
    return piece_type(piece);
}

std::vector<Move> ChessEngine::generateLegalMoves()
{
    std::vector<Move> moves;

    if (position.turn() == WHITE)
    {
        MoveList<WHITE> list(position);
        for (const Move &move : list)
        {
            moves.push_back(move);
        }
    }
    else
    {
        MoveList<BLACK> list(position);
        for (const Move &move : list)
        {
            moves.push_back(move);
        }
    }

    return moves;
}

bool ChessEngine::makeMove(const Move &move)
{
    moveStack.push_back(move);

    if (position.turn() == WHITE)
    {
        position.play<WHITE>(move);
    }
    else
    {
        position.play<BLACK>(move);
    }
    return true;
}

void ChessEngine::unmakeMove()
{
    if (moveStack.empty())
    {
        std::cout << "Move stack is empty, nothing to undo.\n";
        return;
    }

    Move lastMove = moveStack.back();
    moveStack.pop_back();

    Color turnBeforeUndo = position.turn();

    if (turnBeforeUndo == WHITE)
    {
        position.undo<BLACK>(lastMove);
    }
    else
    {
        position.undo<WHITE>(lastMove);
    }
}

Color ChessEngine::getSideToMove() const
{
    return position.turn();
}

bool ChessEngine::isInCheck(Color side) const
{
    if (side == WHITE)
    {
        return position.in_check<WHITE>();
    }
    else
    {
        return position.in_check<BLACK>();
    }
}

bool ChessEngine::isCheckmate() const
{
    if (position.turn() == WHITE)
    {
        PositionManager tempPosition = position;
        MoveList<WHITE> list(tempPosition);
        return position.in_check<WHITE>() && list.size() == 0;
    }
    else
    {
        PositionManager tempPosition = position;
        MoveList<BLACK> list(tempPosition);
        return position.in_check<BLACK>() && list.size() == 0;
    }
}

bool ChessEngine::isStalemate() const
{
    if (position.turn() == WHITE)
    {
        PositionManager tempPosition = position;
        MoveList<WHITE> list(tempPosition);
        return !position.in_check<WHITE>() && list.size() == 0;
    }
    else
    {
        PositionManager tempPosition = position;
        MoveList<BLACK> list(tempPosition);
        return !position.in_check<BLACK>() && list.size() == 0;
    }
}

std::string ChessEngine::moveToString(const Move &move) const
{
    std::stringstream ss;

    Square from = move.from();
    Square to = move.to();

    Piece piece = position.at(from);
    PieceType pieceType = piece_type(piece);

    if (pieceType != PAWN)
    {
        ss << "NBRQK"[pieceType - 1];
    }

    ss << SQUARE_STR[from] << SQUARE_STR[to];

    if (move.flags() == PR_KNIGHT || move.flags() == PC_KNIGHT)
    {
        ss << "n";
    }
    else if (move.flags() == PR_BISHOP || move.flags() == PC_BISHOP)
    {
        ss << "b";
    }
    else if (move.flags() == PR_ROOK || move.flags() == PC_ROOK)
    {
        ss << "r";
    }
    else if (move.flags() == PR_QUEEN || move.flags() == PC_QUEEN)
    {
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
const int MATE_SCORE = 100000;
const int INF = 1000000;

// piece values
const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 300;
const int BISHOP_VALUE = 300;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;

// tempo bonuses
const int MG_TEMPO = 10;
const int EG_TEMPO = 5;

// pair bonuses / penalties
const int BISHOP_PAIR = 30;
const int P_KNIGHT_PAIR = 10;
const int P_ROOK_PAIR = 20;

// game phase weight
int gamePhase;
const int PHASE_KNIGHT = 1;
const int PHASE_BISHOP = 1;
const int PHASE_ROOK = 2;
const int PHASE_QUEEN = 4;

inline int flip_rank(int sq)
{
    return sq ^ 56; // flips vertical rank (0-63)
}

Bitboard ChessEngine::getFriendlyPieces(Color color) const
{
    return (color == WHITE) ? position.all_pieces<WHITE>() : position.all_pieces<BLACK>();
}

int ChessEngine::eval()
{
    if (inEndgame())
    {
        return evalEndgame();
    }

    int gamePhase = game_phase();                                                          // 0 (endgame) to 24 (opening)
    int maxPhase = PHASE_KNIGHT * 4 + PHASE_BISHOP * 4 + PHASE_ROOK * 4 + PHASE_QUEEN * 2; // = 24

    Score white = evaluate_color(WHITE);
    Score black = evaluate_color(BLACK);

    // tempo bonus
    if (position.turn() == WHITE)
    {
        white.mg += MG_TEMPO;
        white.eg += EG_TEMPO;
    }
    else
    {
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

Score ChessEngine::evaluate_color(Color color)
{
    Score eval;

    eval += count_material(color);
    eval += evalPawns(color);
    eval += evalKnights(color);
    eval += evalBishops(color);
    eval += evalRooks(color);
    eval += evalQueens(color);

    eval += evalPawnStructure(color);
    eval += evalKingVulnerability(color);

    return eval;
}

Score ChessEngine::evalPawnStructure(Color color)
{
    Score score;
    Bitboard pawns = position.bitboard_of(color, PAWN);

    for (int i = 0; i < 8; i++)
    { // for doubled pawns
        int count = sparse_pop_count(pawns & MASK_FILE[i]);
        if (count > 1)
            score.mg -= 10 * (count - 1);
        score.eg -= 20 * (count - 1);
    }

    Bitboard isolatedPawns = 0;
    for (int i = 0; i < 8; i++)
    {
        Bitboard file_pawns = pawns & MASK_FILE[i];
        if (file_pawns)
        {
            Bitboard adjacent_files = 0;
            if (i > 0)
                adjacent_files |= MASK_FILE[i - 1];
            if (i < 7)
                adjacent_files |= MASK_FILE[i + 1];

            if (!(pawns & adjacent_files))
                isolatedPawns |= file_pawns;
        }
    }

    score.mg -= 20 * sparse_pop_count(isolatedPawns);
    score.eg -= 10 * sparse_pop_count(isolatedPawns);
    // TODO: add bonus for isolated pawns on the 4th rank
    Bitboard passed_pawns = position.bitboard_of(~color, PAWN); // enemy pawns
    Bitboard passed = 0;

    while (pawns)
    {
        Square sq = pop_lsb(&pawns);
        int file = file_of(sq);

        Bitboard pmask = 0;
        if (color == WHITE)
        {
            pmask = SQUARE_BB[sq];
            for (int r = rank_of(sq) + 1; r <= RANK8; r++)
                pmask |= SQUARE_BB[create_square(File(file), Rank(r))];
        }
        else
        {
            pmask = SQUARE_BB[sq];
            for (int r = rank_of(sq) - 1; r >= RANK1; r--)
                pmask |= SQUARE_BB[create_square(File(file), Rank(r))];
        }

        Bitboard fmask = MASK_FILE[file];
        if (file > 0)
            fmask |= MASK_FILE[file - 1];
        if (file < 7)
            fmask |= MASK_FILE[file + 1];

        if (!(pmask & fmask & passed_pawns))
        {
            passed |= SQUARE_BB[sq];
        }
    }

    while (passed)
    {
        Square sq = pop_lsb(&passed);
        int rank = rank_of(sq);
        int color_based_rank = color == WHITE ? rank : 7 - rank;

        score.mg += 10 * (color_based_rank + 1) * (color_based_rank + 1);
        score.eg += 20 * (color_based_rank + 1) * (color_based_rank + 1);
    }

    return score;
}

Score ChessEngine::evalKingVulnerability(Color color)
{
    Score score;
    Square kingSquare = bsf(position.bitboard_of(color, KING));

    // pawns near king
    Bitboard kingSpace = KING_ATTACKS[kingSquare] | SQUARE_BB[kingSquare];
    Bitboard friendlyPawns = position.bitboard_of(color, PAWN);

    int protection = sparse_pop_count(kingSpace & friendlyPawns);
    score.mg += 10 * protection;

    // penalize enemy near king
    Color enemy = ~color;
    Bitboard enemyKnights = position.bitboard_of(enemy, KNIGHT);
    Bitboard enemyBishops = position.bitboard_of(enemy, BISHOP);
    Bitboard enemyRooks = position.bitboard_of(enemy, ROOK);
    Bitboard enemyQueens = position.bitboard_of(enemy, QUEEN);

    int knightThreats = 0, bishopThreats = 0, rookThreats = 0, queenThreats = 0;

    Bitboard attackCount = position.all_pieces<WHITE>() | position.all_pieces<BLACK>();
    while (enemyKnights)
    {
        Square sq = pop_lsb(&enemyKnights);
        if (KNIGHT_ATTACKS[sq] & kingSpace)
        {
            knightThreats++;
        }
    }
    while (enemyBishops)
    {
        Square sq = pop_lsb(&enemyBishops);
        if (get_bishop_attacks(sq, attackCount) & kingSpace)
        {
            bishopThreats++;
        }
    }
    while (enemyRooks)
    {
        Square sq = pop_lsb(&enemyRooks);
        if (get_rook_attacks(sq, attackCount) & kingSpace)
        {
            rookThreats++;
        }
    }
    while (enemyQueens)
    {
        Square sq = pop_lsb(&enemyQueens);
        if ((get_bishop_attacks(sq, attackCount) | get_rook_attacks(sq, attackCount)) & kingSpace)
        {
            queenThreats++;
        }
    }

    int threatScore = knightThreats * 20 + bishopThreats * 20 + rookThreats * 40 + queenThreats * 80;
    if (threatScore > 0)
    {
        score.mg -= threatScore * threatScore / 50;
    }

    return score;
}

int ChessEngine::count_material(Color color)
{
    int material = 0;

    material += sparse_pop_count(position.bitboard_of(color, PAWN)) * PAWN_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, KNIGHT)) * KNIGHT_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, BISHOP)) * BISHOP_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, ROOK)) * ROOK_VALUE;
    material += sparse_pop_count(position.bitboard_of(color, QUEEN)) * QUEEN_VALUE;

    return material;
}

int ChessEngine::game_phase()
{
    int phase = 0;

    phase += sparse_pop_count(position.bitboard_of(WHITE, KNIGHT) | position.bitboard_of(BLACK, KNIGHT)) * PHASE_KNIGHT;
    phase += sparse_pop_count(position.bitboard_of(WHITE, BISHOP) | position.bitboard_of(BLACK, BISHOP)) * PHASE_BISHOP;
    phase += sparse_pop_count(position.bitboard_of(WHITE, ROOK) | position.bitboard_of(BLACK, ROOK)) * PHASE_ROOK;
    phase += sparse_pop_count(position.bitboard_of(WHITE, QUEEN) | position.bitboard_of(BLACK, QUEEN)) * PHASE_QUEEN;

    return phase;
}

Score ChessEngine::evalPawns(Color color)
{
    Score score;
    Bitboard pawns = position.bitboard_of(color, PAWN);

    while (pawns)
    {
        Square sq = pop_lsb(&pawns);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_PAWN_PST[index];
        score.eg += EG_PAWN_PST[index];

        // TODO: add mobility bonus
    }

    return score;
}

Score ChessEngine::evalKnights(Color color)
{
    Score score;
    Bitboard knights = position.bitboard_of(color, KNIGHT);

    // pair bonus
    if (sparse_pop_count(knights) > 1)
    {
        score.mg -= P_KNIGHT_PAIR;
        score.eg -= P_KNIGHT_PAIR;
    }

    int totalMobility = 0;
    int attack = 0;
    while (knights)
    {
        Square sq = pop_lsb(&knights);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_KNIGHT_PST[index];
        score.eg += EG_KNIGHT_PST[index];

        Bitboard attacks = KNIGHT_ATTACKS[sq];
        Bitboard reachable = attacks & ~getFriendlyPieces(color);
        score += evalKnightMobility(sq, color, reachable);

        /****************************************************************
         *  Collect data about king attacks                              *
         ****************************************************************/
        Color enemy = (color == WHITE) ? BLACK : WHITE;
        Bitboard enemyKingZone = KING_ATTACKS[sq];
        Bitboard attacKing = enemyKingZone & reachable; // hehe get it
        attack += sparse_pop_count(attacKing);
    }

    // attack king bonus
    score.mg += 2 * attack;
    score.eg += 2 * attack;

    return score;
}

Score ChessEngine::evalKnightMobility(Square sq, Color color, Bitboard poss)
{
    Score score;
    int mobility = sparse_pop_count(poss);

    score.mg += 4 * (mobility - 4);
    score.eg += 6 * (mobility - 4);

    return score;
}

Score ChessEngine::evalBishops(Color color)
{
    Score score;
    Bitboard bishops = position.bitboard_of(color, BISHOP);

    // pair bonus
    if (sparse_pop_count(bishops) > 1)
    {
        score.mg += BISHOP_PAIR;
        score.eg += BISHOP_PAIR;
    }

    int totalMobility = 0;
    int attack = 0;
    while (bishops)
    {
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

Score ChessEngine::evalRooks(Color color)
{
    Score score;
    Bitboard rooks = position.bitboard_of(color, ROOK);

    // pair bonus
    if (sparse_pop_count(rooks) > 1)
    {
        score.mg -= P_ROOK_PAIR;
        score.eg -= P_ROOK_PAIR;
    }

    int totalMobility = 0;
    int attack = 0;
    while (rooks)
    {
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

Score ChessEngine::evalQueens(Color color)
{
    Score score;
    Bitboard queens = position.bitboard_of(color, QUEEN);

    int totalMobility = 0;
    int attack = 0;
    while (queens)
    {
        Square sq = pop_lsb(&queens);

        int index = (color == WHITE) ? sq : flip_rank(sq);

        score.mg += MG_QUEEN_PST[index];
        score.eg += EG_QUEEN_PST[index];

        /****************************************************************
         *  A queen should not be developed too early                    *
         ****************************************************************/
        if (position.turn() == WHITE && rank_of(sq) > RANK2)
        {
            if (position.at(B1) == WHITE_KNIGHT)
                score.mg -= 2;
            score.eg -= 2;
            if (position.at(C1) == WHITE_BISHOP)
                score.mg -= 2;
            score.eg -= 2;
            if (position.at(F1) == WHITE_BISHOP)
                score.mg -= 2;
            score.eg -= 2;
            if (position.at(G1) == WHITE_KNIGHT)
                score.mg -= 2;
            score.eg -= 2;
        }

        if (position.turn() == BLACK && rank_of(sq) < RANK7)
        {
            if (position.at(B8) == BLACK_KNIGHT)
                score.mg -= 2;
            score.eg -= 2;
            if (position.at(C8) == BLACK_BISHOP)
                score.mg -= 2;
            score.eg -= 2;
            if (position.at(F8) == BLACK_BISHOP)
                score.mg -= 2;
            score.eg -= 2;
            if (position.at(G8) == BLACK_KNIGHT)
                score.mg -= 2;
            score.eg -= 2;
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

int ChessEngine::search(int depth, int alpha, int beta, bool nullPrune)
{
    if (checkTimeUp())
    {
        return alpha;
    }
    searchStats.nodes++;
    uint64_t hash = position.get_hash();
    Move bestMove = Move();

    if (transpositionTable.count(hash))
    {
        searchStats.hash_hits++;
        TranspositionTableElement &el = transpositionTable[hash];
        if (el.depth >= depth)
        {
            if (el.flag == 0) {
                searchStats.hash_used++;
                return el.score;
            }
            if (el.flag == 1 && el.score >= beta) return el.score;
            if (el.flag == 2 && el.score <= alpha) return el.score;
            bestMove = el.bestMove;
        }
    }

    if (depth == 0)
    {
        return quiescence_search(alpha, beta);
    }

    if (nullPrune && depth >= 3 && !isInCheck(getSideToMove()) && game_phase())
    {
        position.side_to_play = ~position.side_to_play;
        position.game_ply++;
        position.history[position.game_ply].epsq = NO_SQ;

        int score = -search(depth - 3, -beta, -beta + 1, false);

        position.side_to_play = ~position.side_to_play;
        position.game_ply--;

        if (score >= beta)
        {
            searchStats.null_prunes++;
            return beta;
        }
    }

    int old_alpha = alpha;
    std::vector<Move> moves = generateLegalMoves();
    if (moves.size() == 0)
    {
        if (isInCheck(position.turn()))
        {
            return -MATE_SCORE + depth; // checkmate
        }
        return 0; // stalemate
    }

    std::vector<ScoredMove> scoredMoves = orderMoves(moves);
    Move currentBestMove = Move();
    bool first_move = true;
    int moves_searched = 0;

    if (checkTimeUp())
    {
        return alpha;
    }
   
    for (const ScoredMove &sm : scoredMoves)
    {
        makeMove(sm.move);
        moves_searched++;
        searchStats.moves_searched++;
        current_ply++;
        int evaluation = -search(depth - 1, -beta, -alpha, true);
        current_ply--;
        unmakeMove();

        if (evaluation >= beta)
        {
            searchStats.fail_high++;
            if (first_move)
            {
                searchStats.fail_high_first++;
            }
            first_move = false;
            if (!sm.move.is_capture())
            {
                updateKillerMoves(sm.move, current_ply);
                updateHistoryTable(sm.move, depth, position.turn());
            }
            transpositionTable[hash] = {hash, depth, beta, 1, sm.move};
            return beta; // prune branch
        }

        if (evaluation > alpha)
        {
            alpha = evaluation;
            currentBestMove = sm.move;
        }

        if (checkTimeUp())
        {
            return alpha;
        }
    }

    int flag = alpha > old_alpha ? 0 : 2;
    transpositionTable[hash] = {hash, depth, alpha, flag, currentBestMove};

    return alpha;
}

int ChessEngine::quiescence_search(int alpha, int beta, int qdepth)
{

    if (qdepth >= MAX_Q_DEPTH || checkTimeUp()) return eval();
    searchStats.nodes++;
    searchStats.qnodes++;

    int standing_pat_score = eval();

    if (standing_pat_score + 300 < alpha) return alpha;

    if (standing_pat_score >= beta)
    {
        return beta; // prune branch
    }

    if (standing_pat_score > alpha)
    {
        alpha = standing_pat_score;
    }

    std::vector<Move> attacks;
    for (const Move &move : generateLegalMoves())
    {
        if (move.is_capture())
        {
            attacks.push_back(move);
        }
    }
    // sort attacks
    std::sort(attacks.begin(), attacks.end(), [this](const Move &a, const Move &b)
              { return getCaptureScore(a) > getCaptureScore(b); });

    for (const Move &move : attacks)
    {
        makeMove(move);
        // std::cout << "Quiescence search: " << moveToString(move) << " at qdepth " << std::to_string(qdepth);
        int evaluation = -quiescence_search(-beta, -alpha, qdepth + 1);
        unmakeMove();
        if (evaluation >= beta)
        {
            return beta; // prune branch
        }
        if (evaluation > alpha)
        {
            alpha = evaluation;
        }

        if(checkTimeUp())
        {
            return alpha;
        }
    }
    return alpha;
}

int ChessEngine::getCaptureScore(const Move &move)
{
    if (!move.is_capture())
    {
        return 0;
    }

    Color capcolor;
    PieceType captype = getPieceAt(move.to(), capcolor);
    Color attackercolor;
    PieceType attackertype = getPieceAt(move.from(), attackercolor);

    const int vs[7] = {100, 300, 300, 500, 900, 0, 0};
    const int as[7] = {1, 3, 3, 5, 9, 0, 0};

    return vs[captype] * 10 - as[attackertype];
}

std::vector<ScoredMove> ChessEngine::orderMoves(const std::vector<Move> &moves)
{
    std::vector<ScoredMove> scoredMoves;
    Color side = position.turn();

    for (const Move &move : moves)
    {
        int score = 0;

        if (transpositionTable.count(position.get_hash()) && transpositionTable[position.get_hash()].bestMove == move)
        {
            score += 30000;
        }
        else if (move.is_capture())
        {
            score = 20000 + getCaptureScore(move);
        }
        else if (move.flags() >= PR_KNIGHT && move.flags() <= PR_QUEEN)
        {
            score = 15000 + (move.flags() - PR_KNIGHT) * 100;
        }
        else if (move == killer_moves[current_ply][0])
        {
            score = 10000;
        }
        else if (move == killer_moves[current_ply][1])
        {
            score = 9000;
        }
        else
        {
            score = history_table[side][move.from()][move.to()];
        }
        scoredMoves.push_back({move, score});
    }

    std::sort(scoredMoves.begin(), scoredMoves.end(), [](const ScoredMove &a, const ScoredMove &b)
              { return a.score > b.score; });
    return scoredMoves;
}

void ChessEngine::clearTables()
{
    memset(history_table, 0, sizeof(history_table));

    for (int i = 0; i < MAX_PLY; i++)
    {
        killer_moves[i][0] = Move();
        killer_moves[i][1] = Move();
    }

    transpositionTable.clear();
}

void ChessEngine::updateKillerMoves(const Move &move, int ply)
{
    if (move != killer_moves[ply][0])
    {
        killer_moves[ply][1] = killer_moves[ply][0];
        killer_moves[ply][0] = move;
    }
}

void ChessEngine::updateHistoryTable(const Move &move, int depth, Color side)
{
    history_table[side][move.from()][move.to()] += depth * depth;
}

Move ChessEngine::getBestMove(int maxDepth)
{
    Move bookMove = getOpeningBookMove();
    if (bookMove != Move())
    {
        return bookMove;
    }

    PositionManager originalPosition = position;
    current_ply = 0;
    clearTables();

    Move bestMove;
    int bestScore = -INF;

    transpositionTable.clear();

    for (int depth = 1; depth <= maxDepth; depth++)
    {
        int alpha = -INF;
        int beta = INF;
        Move currentBestMove;
        int currentBestScore = -INF;

        std::vector<Move> moves = generateLegalMoves();

        if (bestMove != Move())
        {
            for (size_t i = 0; i < moves.size(); i++)
            {
                if (moves[i] == bestMove)
                {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }

        for (const Move &move : moves)
        {
            makeMove(move);
            current_ply = 1;
            int evaluation = -search(depth - 1, -beta, -alpha, true);
            current_ply = 0;
            unmakeMove();

            if (evaluation > currentBestScore)
            {
                currentBestScore = evaluation;
                currentBestMove = move;

                if (evaluation > alpha)
                {
                    alpha = evaluation;
                }
            }
        }

        bestMove = currentBestMove;
        bestScore = currentBestScore;
        last_search_depth = depth;

        std::cout << "Depth " << depth << ": best move = "
                  << moveToString(bestMove) << ", score = "
                  << bestScore << std::endl;

        if (std::abs(bestScore) > MATE_SCORE - 100)
        {
            break;
        }
    }

    position = originalPosition;

    return bestMove;
}

Move ChessEngine::parseMoveString(const std::string &moveStr)
{
    if (moveStr.length() < 4)
    {
        return Move();
    }

    Square from = static_cast<Square>((moveStr[0] - 'a') + 8 * (moveStr[1] - '1'));
    Square to = static_cast<Square>((moveStr[2] - 'a') + 8 * (moveStr[3] - '1'));

    std::vector<Move> moves = generateLegalMoves();
    for (const Move &move : moves)
    {
        if (move.from() == from && move.to() == to)
        {
            if (moveStr.length() > 4)
            {
                char promo = moveStr[4];
                if (promo == 'n' && move.flags() == PR_KNIGHT)
                    return move;
                if (promo == 'b' && move.flags() == PR_BISHOP)
                    return move;
                if (promo == 'r' && move.flags() == PR_ROOK)
                    return move;
                if (promo == 'q' && move.flags() == PR_QUEEN)
                    return move;
            }
            else
            {
                return move;
            }
        }
    }

    return Move();
}
void ChessEngine::loadOpeningBook(const std::string& filename) {
    openingBook.clear();
    
    if (isPolygotFormat(filename)) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open opening book file: " << filename << std::endl;
            return;
        }
        
        const int ENTRY_SIZE = 16;
        char buffer[ENTRY_SIZE];
        
        PositionManager testPosition;
        
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
            
            resetToStartingPosition();
            
            Move move = polyglotMoveToMove(moveData);
            
            if (move != Move()) { 
                openingBook.push_back({key, move, weight});
            }
        }
        
        file.close();
    }
    else {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open opening book file: " << filename << std::endl;
            return;
        }

        uint64_t hash;
        std::string moveStr;
        int weight;

        while (file >> std::hex >> hash >> moveStr >> weight) {
            Move move = parseMoveString(moveStr);
            openingBook.push_back({hash, move, weight});
        }
        
        file.close();
    }
    
    std::sort(openingBook.begin(), openingBook.end(),
        [](const OpeningBookMove& a, const OpeningBookMove& b) {
            return a.hash < b.hash;
        });
        
    std::cout << "Loaded " << openingBook.size() << " opening book positions" << std::endl;
}

bool ChessEngine::isPolygotFormat(const std::string& filename) {
    return filename.size() > 4 && filename.substr(filename.size() - 4) == ".bin";
}
Move ChessEngine::polyglotMoveToMove(uint16_t moveData) {
    int from = (moveData >> 6) & 0x3F;
    int to = moveData & 0x3F;
    int promotion = (moveData >> 12) & 0x7;
    
    Square fromSq = static_cast<Square>(from);
    Square toSq = static_cast<Square>(to);
    
    std::vector<Move> legalMoves = generateLegalMoves();
    
    for (const Move& move : legalMoves) {
        if (move.from() == fromSq && move.to() == toSq) {
            if (promotion > 0) {
                if ((promotion == 1 && (move.flags() == PR_KNIGHT || move.flags() == PC_KNIGHT)) ||
                    (promotion == 2 && (move.flags() == PR_BISHOP || move.flags() == PC_BISHOP)) ||
                    (promotion == 3 && (move.flags() == PR_ROOK || move.flags() == PC_ROOK)) ||
                    (promotion == 4 && (move.flags() == PR_QUEEN || move.flags() == PC_QUEEN))) {
                    return move;
                }
            }
            else {
                return move;
            }
        }
    }
    
    return Move();
}
Move ChessEngine::getOpeningBookMove() {
    uint64_t currentHash = position.get_hash();
    
    auto compareByHash = [](const OpeningBookMove& a, const OpeningBookMove& b) {
        return a.hash < b.hash;
    };
    
    OpeningBookMove searchKey{currentHash, Move(), 0};
    
    auto range = std::equal_range(openingBook.begin(), openingBook.end(), searchKey, compareByHash);
    
    if (range.first != range.second) {
        int totalWeight = 0;
        for (auto it = range.first; it != range.second; ++it) {
            totalWeight += it->weight;
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, totalWeight - 1);
        int choice = distrib(gen);
        
        int current = 0;
        for (auto it = range.first; it != range.second; ++it) {
            current += it->weight;
            if (current > choice) {
                return it->move;
            }
        }
    }
    
    return Move();
}

bool ChessEngine::inEndgame()
{
    // determine if game in endgame
    bool anyQueens = sparse_pop_count(position.bitboard_of(WHITE, QUEEN) | position.bitboard_of(BLACK, QUEEN)) == 0;

    int totalValuePieces =
        sparse_pop_count(position.bitboard_of(WHITE, KNIGHT)) * KNIGHT_VALUE +
        sparse_pop_count(position.bitboard_of(WHITE, BISHOP)) * BISHOP_VALUE +
        sparse_pop_count(position.bitboard_of(WHITE, ROOK)) * ROOK_VALUE +
        sparse_pop_count(position.bitboard_of(WHITE, QUEEN)) * QUEEN_VALUE +
        sparse_pop_count(position.bitboard_of(BLACK, KNIGHT)) * KNIGHT_VALUE +
        sparse_pop_count(position.bitboard_of(BLACK, BISHOP)) * BISHOP_VALUE +
        sparse_pop_count(position.bitboard_of(BLACK, ROOK)) * ROOK_VALUE +
        sparse_pop_count(position.bitboard_of(BLACK, QUEEN)) * QUEEN_VALUE;

    return anyQueens || totalValuePieces < 1500;
}

int ChessEngine::evalEndgame()
{
    Score score;
    score += evaluate_color(WHITE) - evaluate_color(BLACK);

    Square whiteKing = bsf(position.bitboard_of(WHITE, KING));
    Square blackKing = bsf(position.bitboard_of(BLACK, KING));

    int wkf = file_of(whiteKing);
    int wkr = rank_of(whiteKing);
    int bkf = file_of(blackKing);
    int bkr = rank_of(blackKing);
    int wfd = std::max(3 - wkf, wkf - 4);
    int bfd = std::max(3 - bkf, bkf - 4);
    int wrd = std::max(3 - wkr, wkr - 4);
    int brd = std::max(3 - bkr, bkr - 4);

    int wcd = wfd + wrd;
    int bcd = bfd + brd;

    score.eg += (bcd - wcd) * 10;

    int kingDistance = std::max(std::abs(wkf - bkf), std::abs(wkr - bkr));

    if (sparse_pop_count(position.bitboard_of(WHITE, PAWN) | position.bitboard_of(BLACK, PAWN)) == 0)
    {
        bool pat = (kingDistance % 2 == 0) && (getSideToMove() == BLACK);
        if (pat)
        {
            score.eg += 20;
        }
    }
    return score.eg;
}
Move ChessEngine::getBestMoveWithTime(int time_ms)
{
    Move bookMove = getOpeningBookMove();
    if (bookMove != Move())
    {
        return bookMove;
    }

    PositionManager originalPosition = position;

    current_ply = 0;
    clearTables();

    start_time = SDL_GetTicks();
    allocated_time_ms = time_ms;
    time_up_flag = false;

    Move bestMove;
    int bestScore = -INF;
    
    for (int depth = 1; depth <= MAX_B_DEPTH; depth++)
    {
        if(time_up_flag) break;
        int current_time = SDL_GetTicks();
        int elapsed = current_time - start_time;

        if (depth > 5 && elapsed > time_ms / 2)
            break;

        int alpha = -INF;
        int beta = INF;
        Move currentBestMove;
        int currentBestScore = -INF;

        std::vector<Move> moves = generateLegalMoves();
        if (bestMove != Move())
        {
            for (size_t i = 0; i < moves.size(); i++)
            {
                if (moves[i] == bestMove)
                {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }
        bool depth_completed = true;
        for (const Move &move : moves)
        {
            makeMove(move);
            current_ply = 1;
            int evaluation = -search(depth - 1, -beta, -alpha, true);
            current_ply = 0;
            unmakeMove();

            if (time_up_flag)
            {
                depth_completed = false;
                break;
            }

            if (evaluation > currentBestScore)
            {
                currentBestScore = evaluation;
                currentBestMove = move;

                if (evaluation > alpha)
                {
                    alpha = evaluation;
                }
            }

            current_time = SDL_GetTicks();
            elapsed = current_time - start_time;
            if(time_up_flag) {
                depth_completed = false;
                break;
            }
        }
        if (depth_completed || depth == 1) {
            bestMove = currentBestMove;
            bestScore = currentBestScore;
            last_search_depth = depth;

            std::cout << "Deph " << depth << ": best move = "
                    << moveToString(bestMove) << ", score = "
                    << bestScore << ", time: " << elapsed << "ms" << std::endl;
        }
        if (std::abs(bestScore) > MATE_SCORE - 100) break;
    }

    position = originalPosition;
    last_score = bestScore;
    std::cout << "Search completed, reached depth " << last_search_depth 
              << " in " << (SDL_GetTicks() - start_time) << "ms" << std::endl;

    return bestMove;
}

void ChessEngine::resetSearchStats() {
    searchStats = {0};
    total_nodes = 0;
}

std::vector<AnalysisResult> ChessEngine::runAnalysis(const std::vector<std::string>& positions, int time_per_position_ms) {
    std::vector<AnalysisResult> results;

    for (const std::string& fen : positions) {
        PositionManager::set(fen, position);

        resetSearchStats();
        int64_t start_nodes = total_nodes;
        auto start_time = std::chrono::high_resolution_clock::now();

        Move best_move = getBestMoveWithTime(time_per_position_ms);

        auto end_time = std::chrono::high_resolution_clock::now();  
        double elapsed_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        makeMove(best_move);
        int score = -eval();
        unmakeMove();

        AnalysisResult result;
        result.nodes = searchStats.nodes;
        result.time_ms = elapsed_time;
        result.depth_reached = last_search_depth;
        result.best_move = moveToString(best_move);
        result.score = score;

        results.push_back(result);
    }

    return results;
}

void ChessEngine::printSearchStats() {
    double branching = 0;
    if (searchStats.moves_searched > 0 && searchStats.nodes > 0)
    {
        branching = (double)searchStats.moves_searched / searchStats.nodes;
    }

    double hash_rate = 0;
    if (searchStats.nodes > 0)
    {
        hash_rate = (double)searchStats.hash_hits / searchStats.nodes * 100;
    }

    double cutoff_rate = 0;
    if(searchStats.fail_high > 0)
    {
        cutoff_rate = (double)searchStats.fail_high_first / searchStats.fail_high * 100;
    }

    std::cout << "Search Stats:" << std::endl;
    std::cout << " - Nodes: " << searchStats.nodes << " (" << searchStats.qnodes << " qnodes)" << std::endl;
    std::cout << " - Average branching factor: " << std::fixed << std::setprecision(2) << branching << std::endl;
    std::cout << " - Hash hits: " << searchStats.hash_hits << " (" << hash_rate << "%)" << std::endl;
    std::cout << " - Fail high: (first move cutoffs) " << searchStats.fail_high << " (" << cutoff_rate << "%)" << std::endl;
    std::cout << " - Null prunes: " << searchStats.null_prunes << std::endl;
    std::cout << " - Hash used: " << searchStats.hash_used << std::endl;
    std::cout << " - Moves searched: " << searchStats.moves_searched << std::endl;
    std::cout << " - Hash table size: " << transpositionTable.size() << std::endl;
}

void ChessEngine::perftDivide(int depth) {
    std::vector<Move> moves = generateLegalMoves();
    uint64_t total = 0;
    
    for (const Move& move : moves) {
        makeMove(move);
        uint64_t count = perft(depth - 1);
        unmakeMove();
        
        std::cout << moveToString(move) << ": " << count << std::endl;
        total += count;
    }
    
    std::cout << "\nTotal positions: " << total << std::endl;

    if (position.fen().find("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w") == 0) {
        const uint64_t expectedCounts[] = {
            20,      
            400,
            8902,
            197281,
            4865609,
            119060324,
            3195901860,
            84998978956
        };
        
        if (depth <= 8) {
            bool success = (total == expectedCounts[depth-1]);
            std::cout << "TEST " << (success ? "SUCCESSFUL" : "FAILED") 
                      << "Expected: " << expectedCounts[depth-1] 
                      << "\nGot: " << total << std::endl;
        }
    }
}

uint64_t ChessEngine::perft(int depth) {
    if (depth == 0) return 1;
    
    std::vector<Move> moves = generateLegalMoves();
    uint64_t nodes = 0;
    
    for (const Move& move : moves) {
        makeMove(move);
        nodes += perft(depth - 1);
        unmakeMove();
    }
    
    return nodes;
}

void ChessEngine::testPerft() {
    struct PerftTest {
        std::string fen;
        int depth;
        uint64_t expected;
    };
    
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
        
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t result = perft(test.depth);
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double nps = result * 1000.0 / time_ms;
        
        bool passed_test = result == test.expected;
        
        std::cout << "Position: " << test.fen << std::endl;
        std::cout << "Depth: " << test.depth << std::endl;
        std::cout << "Nodes: " << result << std::endl;
        std::cout << "Expected: " << test.expected << std::endl;
        std::cout << "Time: " << time_ms << "ms" << std::endl;
        std::cout << "Nodes/second: " << (uint64_t)nps << std::endl;
        std::cout << "Result: " << (passed_test ? "PASS" : "FAIL") << std::endl;
        std::cout << "-------------------" << std::endl;
        
        if (passed_test) passed++;
    }
    
    std::cout << "Passed " << passed << "/" << tests.size() << " tests" << std::endl;
}

MatchResult ChessEngine::selfPlayGames(int games, int depth, bool useTimeControl, int msPerMove, bool useOpeningBook) {
    MatchResult result;

    for (int i = 0; i < games; i++) {
        resetToStartingPosition();
        std::vector<Move> moves;
        bool draw = false;
        int winner = -1;
        int half_moves = 0;

        while (winner == -1 && half_moves < 200) {
            Move move;

            if (useOpeningBook && half_moves < 20) {
                move = getOpeningBookMove();
                if (move == Move()) {
                    if (useTimeControl) {
                        move = getBestMoveWithTime(msPerMove);
                    } else {
                        move = getBestMove(depth);
                    }
                }
            } else {
                if (useTimeControl) {
                    move = getBestMoveWithTime(msPerMove);
                } else {
                    move = getBestMove(depth);
                }
            }

            makeMove(move);
            moves.push_back(move);
            half_moves++;

            if(isCheckmate()) {
                winner = getSideToMove() == WHITE ? 2 : 1;
            } else if(isStalemate() || half_moves >= 100) {
                winner = 0;
            }
        }

        if(winner == 0) {
            result.draws++;
        } else if(winner == 1) {
            result.white_wins++;
        } else {
            result.black_wins++;
        }

        std::cout << "Game " << i + 1 << ": ";
        if(winner == 0) {
            std::cout << "Draw" << std::endl;
        } else if(winner == 1) {
            std::cout << "White wins" << std::endl;
        } else if (winner == 2) {
            std::cout << "Black wins" << std::endl;
        } else {
            std::cout << "Error: Invalid winner" << std::endl;
        }
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
    int total = positions.size();
    
    for (const TestPosition& pos : positions) {
        PositionManager::set(pos.fen, position);
        
        Move best_move = getBestMove(8);
        
        std::string from_str = std::string(1, 'a' + file_of(best_move.from())) + std::string(1, '1' + rank_of(best_move.from()));
        std::string to_str = std::string(1, 'a' + file_of(best_move.to())) + std::string(1, '1' + rank_of(best_move.to()));
        
        std::string move_str = from_str + to_str;
        
        if (best_move.flags() == PR_KNIGHT || best_move.flags() == PC_KNIGHT) move_str += "n";
        if (best_move.flags() == PR_BISHOP || best_move.flags() == PC_BISHOP) move_str += "b";
        if (best_move.flags() == PR_ROOK || best_move.flags() == PC_ROOK) move_str += "r";
        if (best_move.flags() == PR_QUEEN || best_move.flags() == PC_QUEEN) move_str += "q";
        
        bool is_correct = (move_str == pos.best_move);
        if (is_correct) correct++;
        
        std::cout << "Position: " << pos.description << std::endl;
        std::cout << "FEN: " << pos.fen << std::endl;
        std::cout << "Expected: " << pos.best_move << std::endl;
        std::cout << "Engine played: " << move_str << std::endl;
        std::cout << "Result: " << (is_correct ? "CORRECT" : "WRONG") << std::endl;
        std::cout << "-------------------" << std::endl;
    }
    
    std::cout << "Test results: " << correct << "/" << total 
              << " correct (" << (correct * 100.0 / total) << "%)" << std::endl;
}

void ChessEngine::uciLoop() {
    std::string line;
    
    std::cout << "id name ChessGS" << std::endl;
    std::cout << "id author James Kaddissi" << std::endl;    

    std::cout << "option name Hash type spin default 64 min 1 max 1024" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 8" << std::endl;
    
    std::cout << "uciok" << std::endl;
    
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        
        if (token == "quit") {
            break;
        } else if (token == "uci") {
            std::cout << "id name ChessGS" << std::endl;
            std::cout << "id author Your Name" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (token == "isready") {
            std::cout << "readyok" << std::endl;
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
            }
            
            if (token == "moves") {
                std::string moveStr;
                while (iss >> moveStr) {
                    Square from = static_cast<Square>((moveStr[0] - 'a') + 8 * (moveStr[1] - '1'));
                    Square to = static_cast<Square>((moveStr[2] - 'a') + 8 * (moveStr[3] - '1'));
                    
                    std::vector<Move> moves = generateLegalMoves();
                    for (const Move& move : moves) {
                        if (move.from() == from && move.to() == to) {
                            if (moveStr.length() > 4) {
                                char promo = moveStr[4];
                                if ((move.flags() >= PR_KNIGHT && move.flags() <= PR_QUEEN) ||
                                    (move.flags() >= PC_KNIGHT && move.flags() <= PC_QUEEN)) {
                                    makeMove(move);
                                    break;
                                }
                            } else {
                                makeMove(move);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (token == "go") {
            int depth = 4;
            int movetime = 0;
            
            while (iss >> token) {
                if (token == "depth") {
                    iss >> depth;
                } else if (token == "movetime") {
                    iss >> movetime;
                }
            }
            
            Move best_move;
            
            if (movetime > 0) {
                best_move = getBestMoveWithTime(movetime);
            } else {
                best_move = getBestMove(depth);
            }
            
            std::string from_str = std::string(1, 'a' + file_of(best_move.from())) + std::string(1, '1' + rank_of(best_move.from()));
            std::string to_str = std::string(1, 'a' + file_of(best_move.to())) + std::string(1, '1' + rank_of(best_move.to()));
            
            std::string move_str = from_str + to_str;
            
            if (best_move.flags() == PR_KNIGHT || best_move.flags() == PC_KNIGHT) move_str += "n";
            if (best_move.flags() == PR_BISHOP || best_move.flags() == PC_BISHOP) move_str += "b";
            if (best_move.flags() == PR_ROOK || best_move.flags() == PC_ROOK) move_str += "r";
            if (best_move.flags() == PR_QUEEN || best_move.flags() == PC_QUEEN) move_str += "q";
            
            std::cout << "bestmove " << move_str << std::endl;
        }
    }
}

bool ChessEngine::checkTimeUp() {
    if ((searchStats.nodes & (nodes_between_checks - 1)) == 0) {
        if (SDL_GetTicks() - start_time > allocated_time_ms * 0.8) {
            time_up_flag = true;
            return true;
        }
    }
    return time_up_flag;
}
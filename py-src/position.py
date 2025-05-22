import random
import chess

import tomllib
import pathlib

with open(pathlib.Path(__file__).parent / "config.toml", "rb") as f:
    cfg = tomllib.load(f)

SYSTEM_PROMPT = cfg["prompt"]["system"]


def board_to_visual(board: chess.Board) -> str:
    rows = []
    for rank in range(7, -1, -1):
        row = f"{rank + 1}  "
        for file in range(8):
            sq = chess.square(file, rank)
            piece = board.piece_at(sq)
            row += (piece.symbol() if piece else ".") + " "
        rows.append(row)
    rows.append("   a b c d e f g h")
    return "\n".join(rows)


def material_balance(board: chess.Board) -> str:
    piece_values = {
        chess.PAWN: 1,
        chess.KNIGHT: 3,
        chess.BISHOP: 3,
        chess.ROOK: 5,
        chess.QUEEN: 9,
    }
    white = sum(
        piece_values[pt] * len(board.pieces(pt, chess.WHITE)) for pt in piece_values
    )
    black = sum(
        piece_values[pt] * len(board.pieces(pt, chess.BLACK)) for pt in piece_values
    )
    diff = white - black
    if diff > 0:
        return f"White +{diff}"
    elif diff < 0:
        return f"Black +{abs(diff)}"
    return "Equal"


def format_position_prompt(
    board: chess.Board,
    candidates: list[str],
    game_history_san: list[str],
) -> str:
    shuffled = candidates[:]
    random.shuffle(shuffled)

    side = "White" if board.turn == chess.WHITE else "Black"

    castling = []
    if board.has_kingside_castling_rights(chess.WHITE):
        castling.append("White O-O")
    if board.has_queenside_castling_rights(chess.WHITE):
        castling.append("White O-O-O")
    if board.has_kingside_castling_rights(chess.BLACK):
        castling.append("Black O-O")
    if board.has_queenside_castling_rights(chess.BLACK):
        castling.append("Black O-O-O")

    history_str = " ".join(game_history_san[-6:]) if game_history_san else "Game start"
    candidates_str = "\n".join(f"  {m}" for m in shuffled)

    return (
        f"Position (FEN): {board.fen()}\n"
        f"\n"
        f"Board:\n{board_to_visual(board)}\n"
        f"\n"
        f"Side to move: {side}\n"
        f"Move number: {board.fullmove_number}\n"
        f"Castling rights: {', '.join(castling) if castling else 'None'}\n"
        f"Material: {material_balance(board)}\n"
        f"Recent moves: {history_str}\n"
        f"In check: {'Yes' if board.is_check() else 'No'}\n"
        f"\n"
        f"Candidate moves to rank (in no particular order):\n{candidates_str}\n"
        f"\n"
        f"Rank these moves from best to worst for {side}."
    )


def build_prompt_messages(
    board: chess.Board,
    candidates: list[str],
    game_history_san: list[str],
) -> list[dict]:
    return [
        {"role": "system", "content": SYSTEM_PROMPT},
        {
            "role": "user",
            "content": format_position_prompt(board, candidates, game_history_san),
        },
    ]

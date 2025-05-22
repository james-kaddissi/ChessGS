import tomllib
import pathlib
import chess
from stockfish import Stockfish

with open(pathlib.Path(__file__).parent / "config.toml", "rb") as f:
    cfg = tomllib.load(f)

STOCKFISH_PATH = cfg["paths"]["stockfish_path"]
SF_DATASET_DEPTH = cfg["dataset"]["sf_dataset_depth"]
NUM_CANDIDATES = cfg["dataset"]["num_candidates"]


def make_stockfish(depth: int = SF_DATASET_DEPTH) -> Stockfish:
    return Stockfish(
        path=STOCKFISH_PATH,
        parameters={"Threads": 4, "Hash": 256},
        depth=depth,
    )


def get_top_candidates(
    sf: Stockfish,
    fen: str,
    n: int = NUM_CANDIDATES,
) -> tuple[list[str], list[int]]:
    if not sf.is_fen_valid(fen):
        raise ValueError(f"Invalid FEN: {fen}")

    sf.set_fen_position(fen)
    top = sf.get_top_moves(n)

    if not top:
        return [], []

    board = chess.Board(fen)
    sign = 1 if board.turn == chess.WHITE else -1

    moves, evals = [], []
    for entry in top:
        moves.append(entry["Move"])
        if entry.get("Mate") is not None:
            cp = (10000 if entry["Mate"] > 0 else -10000) * sign
        else:
            cp = entry.get("Centipawn", 0) * sign
        evals.append(cp)

    return moves, evals


def is_interesting_position(
    board: chess.Board,
    candidates: list[str],
    evals: list[int],
) -> bool:
    if len(candidates) < 3:
        return False
    if board.is_checkmate() or board.is_stalemate():
        return False
    if len(evals) >= 2 and abs(evals[0] - evals[1]) > 500:
        return False
    if len(evals) >= 2 and abs(evals[0] - evals[1]) < 10:
        return False
    return True

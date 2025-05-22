import os
import random
import logging

import chess
import chess.pgn
from datasets import Dataset
from position import format_position_prompt
from stockfish_oracle import make_stockfish, get_top_candidates, is_interesting_position
import tomllib
import pathlib

with open(pathlib.Path(__file__).parent / "config.toml", "rb") as f:
    cfg = tomllib.load(f)

PGN_DIR = cfg["paths"]["pgn_dir"]
DATASET_PATH = cfg["paths"]["dataset_path"]
MAX_GAMES = cfg["dataset"]["max_games"]
POSITIONS_PER_GAME = cfg["dataset"]["positions_per_game"]
MIN_ELO = cfg["dataset"]["min_elo"]
SYSTEM_PROMPT = cfg["prompt"]["system"]

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)


def build_dataset() -> None:
    sf = make_stockfish()
    formatted_data = []
    game_counter = 0
    total_positions = 0

    pgn_files = [
        os.path.join(PGN_DIR, f) for f in os.listdir(PGN_DIR) if f.endswith(".pgn")
    ]
    if not pgn_files:
        raise FileNotFoundError(f"No .pgn files found in {PGN_DIR}")

    log.info(f"Found {len(pgn_files)} PGN file(s). Target: {MAX_GAMES} games.")

    for pgn_file in pgn_files:
        if game_counter >= MAX_GAMES:
            break

        with open(pgn_file, "r", encoding="utf-8", errors="ignore") as f:
            while game_counter < MAX_GAMES:
                game = chess.pgn.read_game(f)
                if game is None:
                    break

                try:
                    white_elo = int(game.headers.get("WhiteElo", 0))
                    black_elo = int(game.headers.get("BlackElo", 0))
                    if white_elo < MIN_ELO or black_elo < MIN_ELO:
                        continue
                except (ValueError, TypeError):
                    pass

                moves = list(game.mainline_moves())
                if len(moves) < 20:
                    continue

                sample_range = list(range(12, min(50, len(moves) - 2)))
                if len(sample_range) < POSITIONS_PER_GAME:
                    continue

                sampled = sorted(random.sample(sample_range, POSITIONS_PER_GAME))

                board = game.board()
                san_history = []

                for ply_idx, move in enumerate(moves):
                    san_history.append(board.san(move))
                    board.push(move)

                    if ply_idx not in sampled:
                        continue

                    fen = board.fen()

                    try:
                        candidates, evals = get_top_candidates(sf, fen)
                    except Exception as e:
                        log.warning(f"Stockfish error at ply {ply_idx}: {e}")
                        continue

                    if not is_interesting_position(board, candidates, evals):
                        continue

                    user_prompt = format_position_prompt(
                        board=board,
                        candidates=candidates,
                        game_history_san=san_history,
                    )

                    formatted_data.append(
                        {
                            "prompt": [
                                {"role": "system", "content": SYSTEM_PROMPT},
                                {"role": "user", "content": user_prompt},
                            ],
                            "fen": fen,
                            "sf_ranking": candidates,
                            "sf_evals": evals,
                            "candidates": candidates,
                        }
                    )
                    total_positions += 1

                game_counter += 1
                if game_counter % 500 == 0:
                    log.info(
                        f"  {game_counter} games processed, {total_positions} positions collected"
                    )

    dataset = Dataset.from_list(formatted_data)
    dataset.to_json(DATASET_PATH)
    log.info(
        f"Saved {len(formatted_data)} examples from {game_counter} games → {DATASET_PATH}"
    )


if __name__ == "__main__":
    build_dataset()

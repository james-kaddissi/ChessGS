import re
from typing import Optional

import chess
import numpy as np
from scipy.stats import kendalltau


def extract_best_move(text: str) -> Optional[str]:
    match = re.search(
        r"<best>\s*([a-h][1-8][a-h][1-8][qrbn]?)\s*</best>",
        text,
        re.IGNORECASE,
    )
    return match.group(1).lower() if match else None


def extract_ranking(text: str, candidates: list[str]) -> Optional[list[str]]:
    block = re.search(r"<ranking>(.*?)</ranking>", text, re.DOTALL)
    if not block:
        return None

    found = []
    for line in block.group(1).strip().splitlines():
        for cand in candidates:
            if cand.lower() in line.lower() and cand not in found:
                found.append(cand)
                break

    return found if len(found) >= 2 else None


def _get_text(completion) -> str:
    if isinstance(completion, list):
        return completion[0]["content"]
    if isinstance(completion, dict):
        return completion.get("content", "")
    return str(completion)


def format_reward(completions, **kwargs) -> list[float]:
    rewards = []
    for completion in completions:
        text = _get_text(completion)
        score = 0.0
        if re.search(r"<reasoning>.*?</reasoning>", text, re.DOTALL):
            score += 0.3
        if re.search(r"<ranking>.*?</ranking>", text, re.DOTALL):
            score += 0.4
        if re.search(
            r"<best>\s*[a-h][1-8][a-h][1-8][qrbn]?\s*</best>", text, re.IGNORECASE
        ):
            score += 0.3
        rewards.append(score)
    return rewards


def legality_reward(completions, fen, **kwargs) -> list[float]:
    fens = fen if isinstance(fen, list) else [fen] * len(completions)
    rewards = []

    for completion, f in zip(completions, fens):
        text = _get_text(completion)
        move_str = extract_best_move(text)

        if move_str is None:
            rewards.append(-1.0)
            continue

        try:
            board = chess.Board(f)
            move = chess.Move.from_uci(move_str)
            rewards.append(1.0 if move in board.legal_moves else -1.0)
        except Exception:
            rewards.append(-1.0)

    return rewards


def ranking_reward(completions, sf_ranking, candidates, **kwargs) -> list[float]:
    sf_rankings = (
        sf_ranking
        if isinstance(sf_ranking[0], list)
        else [sf_ranking] * len(completions)
    )
    all_candidates = (
        candidates
        if isinstance(candidates[0], list)
        else [candidates] * len(completions)
    )

    rewards = []
    for completion, sf_rank, cands in zip(completions, sf_rankings, all_candidates):
        text = _get_text(completion)
        llm_ranking = extract_ranking(text, cands)

        if llm_ranking is None:
            rewards.append(-0.5)
            continue

        sf_idx = {move: i for i, move in enumerate(sf_rank)}
        common = [m for m in llm_ranking if m in sf_idx]

        if len(common) < 2:
            rewards.append(-0.5)
            continue

        llm_order = list(range(len(common)))
        sf_order = [sf_idx[m] for m in common]

        tau, _ = kendalltau(llm_order, sf_order)
        rewards.append(float(tau))

    return rewards


def best_move_quality_reward(
    completions, fen, sf_evals, candidates, **kwargs
) -> list[float]:
    fens = fen if isinstance(fen[0], str) else [fen] * len(completions)
    all_evals = (
        sf_evals
        if isinstance(sf_evals[0], (list, tuple))
        else [sf_evals] * len(completions)
    )
    all_cands = (
        candidates
        if isinstance(candidates[0], list)
        else [candidates] * len(completions)
    )

    rewards = []
    for completion, f, evals, cands in zip(completions, fens, all_evals, all_cands):
        text = _get_text(completion)
        move_str = extract_best_move(text)

        if move_str is None or not evals or not cands:
            rewards.append(-0.5)
            continue

        eval_map = dict(zip(cands, evals))

        if move_str not in eval_map:
            rewards.append(-1.0)
            continue

        best_eval = evals[0]
        chosen_eval = eval_map[move_str]
        loss = max(0, best_eval - chosen_eval)

        rewards.append(float(np.exp(-loss / 100.0)))

    return rewards

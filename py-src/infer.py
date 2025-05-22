import logging
import re
from typing import Optional

import chess
import torch
from peft import PeftModel
from transformers import AutoTokenizer, AutoModelForCausalLM

from position import build_prompt_messages
from rewards import extract_best_move, extract_ranking

import tomllib
import pathlib

with open(pathlib.Path(__file__).parent / "config.toml", "rb") as f:
    cfg = tomllib.load(f)
    
MODEL_NAME = cfg["model"]["name"]
OUTPUT_DIR = cfg["paths"]["output_dir"]

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)

CHECKPOINT_PATH = f"{OUTPUT_DIR}/checkpoint-final"

MAX_NEW_TOKENS = 512


def load_model(checkpoint_path: str = CHECKPOINT_PATH):
    log.info(f"Loading base model: {MODEL_NAME}")
    base = AutoModelForCausalLM.from_pretrained(
        MODEL_NAME,
        device_map="auto",
        torch_dtype=torch.bfloat16,
    )

    log.info(f"Applying LoRA checkpoint: {checkpoint_path}")
    model = PeftModel.from_pretrained(base, checkpoint_path)
    model.eval()

    tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)
    tokenizer.pad_token = tokenizer.eos_token

    return model, tokenizer


def rank_moves(
    model,
    tokenizer,
    board: chess.Board,
    candidates: list[str],
    game_history_san: list[str],
) -> dict:
    messages = build_prompt_messages(
        board=board,
        candidates=candidates,
        game_history_san=game_history_san,
    )

    input_ids = tokenizer.apply_chat_template(
        messages,
        return_tensors="pt",
        add_generation_prompt=True,
    ).to(model.device)

    with torch.no_grad():
        output_ids = model.generate(
            input_ids,
            max_new_tokens=MAX_NEW_TOKENS,
            do_sample=False,
        )

    raw = tokenizer.decode(
        output_ids[0][input_ids.shape[1] :],
        skip_special_tokens=True,
    )

    best = extract_best_move(raw)
    ranking = extract_ranking(raw, candidates)
    reasoning_match = re.search(r"<reasoning>(.*?)</reasoning>", raw, re.DOTALL)

    if best is None or best not in candidates:
        log.warning(
            f"LLM best move '{best}' not in candidates — falling back to {candidates[0]}"
        )
        best = candidates[0]

    return {
        "best_move": best,
        "ranking": ranking or candidates,
        "reasoning": reasoning_match.group(1).strip() if reasoning_match else "",
        "raw": raw,
    }
    
if __name__ == "__main__":
    model, tokenizer = load_model()

    board = chess.Board()
    board.push_uci("e2e4")
    board.push_uci("e7e5")
    board.push_uci("g1f3")

    candidates = ["b8c6", "g8f6", "d7d6", "f7f5", "d7d5"]
    history = ["e4", "e5", "Nf3"]

    result = rank_moves(
        model=model,
        tokenizer=tokenizer,
        board=board,
        candidates=candidates,
        game_history_san=history,
    )

    print(f"\nBest move:  {result['best_move']}")
    print(f"Ranking:    {result['ranking']}")
    print(f"\nReasoning:\n{result['reasoning']}")
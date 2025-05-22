import logging

import torch
from datasets import load_dataset
from transformers import AutoTokenizer, AutoModelForCausalLM, BitsAndBytesConfig
import pathlib
import tomllib

with open(pathlib.Path(__file__).parent / "config.toml", "rb") as f:
    cfg = tomllib.load(f)

MODEL_NAME = cfg["model"]["name"]
DATASET_PATH = cfg["paths"]["dataset_path"]

from rewards import (
    format_reward,
    legality_reward,
    ranking_reward,
    best_move_quality_reward,
)

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)

NUM_SAMPLES = 3


def test_rewards() -> None:
    log.info(f"Loading model: {MODEL_NAME}")

    bnb_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_compute_dtype=torch.bfloat16,
    )
    model = AutoModelForCausalLM.from_pretrained(
        MODEL_NAME,
        device_map="auto",
        quantization_config=bnb_config,
        torch_dtype=torch.bfloat16,
    )
    tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)
    tokenizer.pad_token = tokenizer.eos_token

    log.info(f"Loading dataset: {DATASET_PATH}")
    dataset = load_dataset("json", data_files=DATASET_PATH)["train"]
    samples = dataset.select(range(NUM_SAMPLES))

    completions = []
    fens = []
    sf_rankings = []
    sf_evals_list = []
    candidates_list = []

    for i in range(NUM_SAMPLES):
        sample = samples[i]

        prompt = tokenizer.apply_chat_template(
            sample["prompt"],
            tokenize=False,
            add_generation_prompt=True,
        )
        inputs = tokenizer(prompt, return_tensors="pt").to(model.device)

        with torch.no_grad():
            outputs = model.generate(**inputs, max_new_tokens=512, do_sample=False)

        generated = tokenizer.decode(
            outputs[0][inputs["input_ids"].shape[1] :],
            skip_special_tokens=True,
        )

        log.info(f"\n{'='*60}\nSample {i} completion:\n{generated}\n{'='*60}")

        completions.append([{"content": generated}])
        fens.append(sample["fen"])
        sf_rankings.append(sample["sf_ranking"])
        sf_evals_list.append(sample["sf_evals"])
        candidates_list.append(sample["candidates"])

    print("\n--- Reward Scores ---")
    print(f"Format:          {format_reward(completions)}")
    print(f"Legality:        {legality_reward(completions, fen=fens)}")
    print(
        f"Ranking (tau):   {ranking_reward(completions, sf_ranking=sf_rankings, candidates=candidates_list)}"
    )
    print(
        f"Best move qual:  {best_move_quality_reward(completions, fen=fens, sf_evals=sf_evals_list, candidates=candidates_list)}"
    )


if __name__ == "__main__":
    test_rewards()

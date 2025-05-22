import random
import logging

import torch
from datasets import load_dataset, Dataset
from transformers import AutoTokenizer, AutoModelForCausalLM, BitsAndBytesConfig
from peft import LoraConfig
from trl import GRPOConfig, GRPOTrainer
import pathlib
import tomllib

with open(pathlib.Path(__file__).parent / "config.toml", "rb") as f:
    cfg = tomllib.load(f)
MODEL_NAME = cfg["model"]["name"]
OUTPUT_DIR = cfg["paths"]["output_dir"]
RUN_NAME = cfg["paths"]["run_name"]
LEARNING_RATE = cfg["training"]["learning_rate"]
WEIGHT_DECAY = cfg["training"]["weight_decay"]
WARMUP_RATIO = cfg["training"]["warmup_ratio"]
BATCH_SIZE = cfg["training"]["batch_size"]
GRAD_ACCUM_STEPS = cfg["training"]["grad_accum_steps"]
NUM_GENERATIONS = cfg["training"]["num_generations"]
MAX_PROMPT_LENGTH = cfg["training"]["max_prompt_length"]
MAX_COMPLETION_LEN = cfg["training"]["max_completion_len"]
NUM_EPOCHS = cfg["training"]["num_epochs"]
SAVE_STEPS = cfg["training"]["save_steps"]
MAX_GRAD_NORM = cfg["training"]["max_grad_norm"]
REWARD_WEIGHTS = cfg["rewards"]["weights"]
LORA_R = cfg["lora"]["r"]
LORA_ALPHA = cfg["lora"]["alpha"]
LORA_DROPOUT = cfg["lora"]["dropout"]
SAMPLE_SIZE = cfg["training"]["sample_size"]
DATASET_PATH = cfg["paths"]["dataset_path"]
from rewards import (
    format_reward,
    legality_reward,
    ranking_reward,
    best_move_quality_reward,
)

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)


def train() -> None:
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
        attn_implementation="flash_attention_2",
    )

    tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)
    tokenizer.pad_token = tokenizer.eos_token
    tokenizer.padding_side = "left"

    log.info(f"CUDA available: {torch.cuda.is_available()}")

    log.info(f"Loading dataset: {DATASET_PATH}")
    dataset = load_dataset("json", data_files=DATASET_PATH)["train"]

    if SAMPLE_SIZE is not None:
        indices = random.sample(range(len(dataset)), min(SAMPLE_SIZE, len(dataset)))
        dataset = dataset.select(indices)

    log.info(f"Training on {len(dataset)} examples")

    training_args = GRPOConfig(
        output_dir=OUTPUT_DIR,
        run_name=RUN_NAME,
        learning_rate=LEARNING_RATE,
        adam_beta1=0.9,
        adam_beta2=0.99,
        weight_decay=WEIGHT_DECAY,
        warmup_ratio=WARMUP_RATIO,
        lr_scheduler_type="cosine",
        logging_steps=1,
        bf16=True,
        per_device_train_batch_size=BATCH_SIZE,
        gradient_accumulation_steps=GRAD_ACCUM_STEPS,
        num_generations=NUM_GENERATIONS,
        max_prompt_length=MAX_PROMPT_LENGTH,
        max_completion_length=MAX_COMPLETION_LEN,
        num_train_epochs=NUM_EPOCHS,
        save_steps=SAVE_STEPS,
        max_grad_norm=MAX_GRAD_NORM,
        report_to="wandb",
        log_on_each_node=False,
        reward_weights=REWARD_WEIGHTS,
    )

    peft_config = LoraConfig(
        r=LORA_R,
        lora_alpha=LORA_ALPHA,
        target_modules=[
            "q_proj",
            "k_proj",
            "v_proj",
            "o_proj",
            "up_proj",
            "down_proj",
            "gate_proj",
        ],
        task_type="CAUSAL_LM",
        lora_dropout=LORA_DROPOUT,
        bias="none",
    )

    trainer = GRPOTrainer(
        model=model,
        processing_class=tokenizer,
        reward_funcs=[
            format_reward,
            legality_reward,
            ranking_reward,
            best_move_quality_reward,
        ],
        args=training_args,
        train_dataset=dataset,
        peft_config=peft_config,
    )

    log.info("Starting training...")
    trainer.train()
    log.info(f"Done. Checkpoint saved to {OUTPUT_DIR}")


if __name__ == "__main__":
    train()

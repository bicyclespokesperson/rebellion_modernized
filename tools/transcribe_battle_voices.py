#!/usr/bin/env -S uv run
"""Transcribe the original voice clips locally with Whisper (no network service; the model weights download once).

Reads the WAVEs written by `tools/battle_audio_tool.py extract` and writes one TSV per module (wave id, seconds, text)
under work/battle_audio/transcripts. The result is the evidence behind the voice table in docs/battle_audio.md.
"""
# /// script
# requires-python = ">=3.11,<3.13"
# dependencies = ["faster-whisper>=1.1", "numpy>=2", "scipy>=1.13"]
# ///
from __future__ import annotations

import argparse
import sys
import wave
from pathlib import Path

import numpy as np
from faster_whisper import WhisperModel
from scipy.signal import resample_poly

REPO = Path(__file__).resolve().parent.parent
WHISPER_RATE = 16000
LEAD_SECONDS = 0.3
TAIL_SECONDS = 0.5
VOCABULARY = (
    "Star Wars Rebellion space battle voice. Task Force one, two, three, four, five, six, seven, eight. "
    "Red, Blue, Green, Gold group. Death Star, shields, hyperspace, withdraw, fighters, recover, maneuver, "
    "acknowledged, engaging targets, requesting orders."
)


def load_clip(path: Path) -> np.ndarray:
    with wave.open(str(path)) as reader:
        rate, width = reader.getframerate(), reader.getsampwidth()
        raw = reader.readframes(reader.getnframes())
    if width == 1:
        samples = (np.frombuffer(raw, np.uint8).astype(np.float32) - 128) / 128
    else:
        samples = np.frombuffer(raw, "<i2").astype(np.float32) / 32768
    resampled = resample_poly(samples, WHISPER_RATE, rate).astype(np.float32)
    peak = float(np.abs(resampled).max())
    if peak > 0:
        resampled *= 0.9 / peak
    silence = lambda seconds: np.zeros(int(seconds * WHISPER_RATE), np.float32)
    return np.concatenate([silence(LEAD_SECONDS), resampled, silence(TAIL_SECONDS)])


def transcribe(model: WhisperModel, clip: np.ndarray) -> str:
    segments, _ = model.transcribe(
        clip, language="en", initial_prompt=VOCABULARY, beam_size=5, temperature=0.0,
        condition_on_previous_text=False)
    return " ".join(segment.text.strip() for segment in segments).strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--wav-dir", type=Path, default=REPO / "work" / "battle_audio" / "wav")
    parser.add_argument("--out", type=Path, default=REPO / "work" / "battle_audio" / "transcripts")
    parser.add_argument("--model", default="medium.en")
    parser.add_argument("--modules", nargs="+", default=["voicefxa", "voicefxe"])
    parser.add_argument("--limit", type=int, default=0, help="transcribe only the first N clips of each module")
    args = parser.parse_args()

    model = WhisperModel(args.model, device="cpu", compute_type="int8")
    args.out.mkdir(parents=True, exist_ok=True)
    for module in args.modules:
        paths = sorted((args.wav_dir / module).glob("*.wav"), key=lambda p: int(p.stem))
        rows = []
        for path in paths[: args.limit or None]:
            clip = load_clip(path)
            seconds = len(clip) / WHISPER_RATE - LEAD_SECONDS - TAIL_SECONDS
            rows.append(f"{path.stem}\t{seconds:.2f}\t{transcribe(model, clip)}")
            print(module, rows[-1], flush=True)
        (args.out / f"{module}.{args.model}.tsv").write_text("\n".join(rows) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

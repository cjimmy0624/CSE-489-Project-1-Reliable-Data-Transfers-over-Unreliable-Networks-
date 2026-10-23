# Scripts Guide

This document explains the helper scripts in `student/scripts/` and how to use them.

## `scripts/test_local.sh`

Purpose: quick local smoke test. It builds the project, runs the emulator, starts a receiver and sender, and compares file hashes.

Command:
```bash
./scripts/test_local.sh
```

Notes:
- Uses GBN binaries (`receiver_gbn`, `sender_gbn`) with a small window.
- Creates a temporary input file and removes it at the end.
- Prints SHA-256 hashes for input and output files.

## `scripts/run_reliable.py`

Purpose: run a batch of test cases and write results to `tmp_reliable/results.jsonl`.

Command:
```bash
python scripts/run_reliable.py --mode gbn
```

Options:
- `--mode`: `gbn`, `sr`, `sr_fast`, or `basic`.

What it does:
- Builds the project.
- Generates a random input file.
- Runs a matrix of scenarios (loss, delay, window size).
- Saves logs and outputs under `tmp_reliable/`.
- Writes JSONL results with metrics like goodput, retransmission rate, and hash correctness.

## `scripts/run_reliable_one.py`

Purpose: run a single test case with configurable parameters.

Command:
```bash
python scripts/run_reliable_one.py --mode gbn --loss 0.02 --delay_ms 50 --reorder 0.0 --win 20
```

Options:
- `--mode`: `gbn`, `sr`, `sr_fast`, or `basic`.
- `--loss`: loss rate.
- `--delay_ms`: base delay in ms.
- `--reorder`: reorder probability.
- `--win`: window size.

Outputs:
- Writes one JSONL record to `tmp_reliable/results.jsonl`.
- Saves logs and outputs under `tmp_reliable/`.

## `scripts/process_reliable_results.py`

Purpose: generate plots from a JSONL results file.

Command:
```bash
python scripts/process_reliable_results.py --input tmp_reliable/results.jsonl
```

Options:
- `--input`: path to the JSONL results (default: `tmp_reliable/results.jsonl`).
- `--out_dir`: output directory for PNG plots (default: `tmp_reliable/plots`).
- `--pdf`: optional PDF path (default: `out_dir/reliable_plots.pdf`).

Outputs:
- PNG plots for goodput and retransmission rate across scenarios.
- An aggregated PDF with all plots.

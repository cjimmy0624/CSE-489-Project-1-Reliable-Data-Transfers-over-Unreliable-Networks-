#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time


ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TMP_DIR = os.path.join(ROOT_DIR, "tmp_reliable")

FILE_SIZE_BYTES = 307200
TIMEOUT_MS = 500
SENDER_TIMEOUT_SEC = 60
RECEIVER_TIMEOUT_SEC = 60
RATE_KBPS = 1500

BASE_LOSS = 0.01
BASE_DELAY = 50
BASE_REORDER = 0.00
BASE_WIN = 20


def write_random_file(path, size_bytes):
    with open(path, "wb") as f:
        f.write(os.urandom(size_bytes))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_sender_stdout(stdout_text):
    out = {}
    for line in stdout_text.splitlines():
        if "=" not in line:
            continue
        key, val = line.split("=", 1)
        out[key.strip()] = val.strip()
    return out


def run_case(mode, sender_bin, receiver_bin, loss, delay_ms, reorder, win, input_file):
    label = f"one_l{loss}_d{delay_ms}_r{reorder}_w{win}"
    out_dir = os.path.join(TMP_DIR, "outputs")
    log_dir = os.path.join(TMP_DIR, "logs")
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(log_dir, exist_ok=True)
    out_file = os.path.join(out_dir, f"out_{label}.bin")
    sender_log = os.path.join(log_dir, f"sender_{label}.log")
    sender_err = os.path.join(log_dir, f"sender_{label}.err")

    emulator = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, "emulator.py"),
         "--loss", str(loss),
         "--delay_ms", str(delay_ms),
         "--reorder", str(reorder),
         "--rate_kbps", str(RATE_KBPS),
         "--seed", "1"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(0.2)

    receiver_cmd = [
        receiver_bin,
        "--listen", "10001",
        "--peer_ip", "127.0.0.1",
        "--peer_port", "10000",
        "--out", out_file,
    ]
    if receiver_bin.endswith("_sr"):
        receiver_cmd.extend(["--win", str(win)])

    receiver = subprocess.Popen(
        receiver_cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(0.1)

    sender_cmd = [
        sender_bin,
        "--listen", "10000",
        "--peer_ip", "127.0.0.1",
        "--peer_port", "10001",
        "--in", input_file,
        "--win", str(win),
        "--timeout", str(TIMEOUT_MS),
    ]
    if mode == "sr_fast":
        sender_cmd.append("--fast_retx")

    sender = subprocess.Popen(
        sender_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    sender_rc = 0
    try:
        sender_out, sender_err_text = sender.communicate(timeout=SENDER_TIMEOUT_SEC)
    except subprocess.TimeoutExpired:
        sender.kill()
        sender_out, sender_err_text = sender.communicate()
        sender_rc = 124
    else:
        sender_rc = sender.returncode

    with open(sender_log, "w", encoding="utf-8") as f:
        f.write(sender_out)
    with open(sender_err, "w", encoding="utf-8") as f:
        f.write(sender_err_text)

    receiver_rc = 0
    try:
        receiver.wait(timeout=RECEIVER_TIMEOUT_SEC)
        receiver_rc = receiver.returncode
    except subprocess.TimeoutExpired:
        receiver.kill()
        receiver.wait()
        receiver_rc = 124

    emulator.kill()
    emulator.wait()

    hash_ok = 0
    if os.path.exists(out_file):
        h1 = sha256_file(input_file)
        h2 = sha256_file(out_file)
        if h1 == h2:
            hash_ok = 1

    stats = parse_sender_stdout(sender_out)
    data_sent = int(stats.get("DATA_SENT_PKTS", "0") or 0)
    data_retx = int(stats.get("DATA_RETX_PKTS", "0") or 0)
    retx_rate = (data_retx / data_sent) if data_sent > 0 else 0.0
    return {
        "mode": mode,
        "loss": loss,
        "delay_ms": delay_ms,
        "reorder": reorder,
        "win": win,
        "timeout_ms": TIMEOUT_MS,
        "file_bytes": FILE_SIZE_BYTES,
        "rate_kbps": RATE_KBPS,
        "hash_ok": hash_ok,
        "goodput_kbps": float(stats.get("GOODPUT_KBPS", "0") or 0),
        "data_sent": data_sent,
        "data_retx": data_retx,
        "retx_rate": retx_rate,
        "ack_rcvd": int(stats.get("ACK_RCVD_PKTS", "0") or 0),
        "elapsed_ms": int(float(stats.get("ELAPSED_MS", "0") or 0)),
        "sender_rc": sender_rc,
        "receiver_rc": receiver_rc,
    }


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "help":
        sys.argv[1] = "--help"

    p = argparse.ArgumentParser(description="Run a single GBN/SR test with defaults")
    p.add_argument("--mode", choices=["gbn", "sr", "sr_fast", "basic"], default="gbn")
    p.add_argument("--loss", type=float, default=BASE_LOSS)
    p.add_argument("--delay_ms", type=int, default=BASE_DELAY)
    p.add_argument("--reorder", type=float, default=BASE_REORDER)
    p.add_argument("--win", type=int, default=BASE_WIN)
    args = p.parse_args()

    subprocess.run(["make", "-s"], cwd=ROOT_DIR, check=True)

    if args.mode == "gbn":
        sender_bin = os.path.join(ROOT_DIR, "sender_gbn")
        receiver_bin = os.path.join(ROOT_DIR, "receiver_gbn")
    elif args.mode == "basic":
        sender_bin = os.path.join(ROOT_DIR, "sender_basic")
        receiver_bin = os.path.join(ROOT_DIR, "receiver_basic")
    else:
        sender_bin = os.path.join(ROOT_DIR, "sender_sr")
        receiver_bin = os.path.join(ROOT_DIR, "receiver_sr")

    os.makedirs(TMP_DIR, exist_ok=True)
    os.makedirs(os.path.join(TMP_DIR, "outputs"), exist_ok=True)
    os.makedirs(os.path.join(TMP_DIR, "logs"), exist_ok=True)
    input_file = os.path.join(TMP_DIR, "input.bin")
    write_random_file(input_file, FILE_SIZE_BYTES)

    print("Defaults:")
    print(
        f"loss={BASE_LOSS} delay={BASE_DELAY} reorder={BASE_REORDER} "
        f"win={BASE_WIN} rate_kbps={RATE_KBPS} timeout_ms={TIMEOUT_MS} "
        f"file_bytes={FILE_SIZE_BYTES}"
    )

    record = run_case(
        args.mode,
        sender_bin,
        receiver_bin,
        args.loss,
        args.delay_ms,
        args.reorder,
        args.win,
        input_file,
    )

    results_jsonl = os.path.join(TMP_DIR, "results.jsonl")
    with open(results_jsonl, "w", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=True) + "\n")

    print(f"Mode: {args.mode}")
    print(f"Params: loss={args.loss} delay={args.delay_ms} reorder={args.reorder} win={args.win}")
    print(f"JSONL results: {results_jsonl}")
    print(f"Logs and outputs: {TMP_DIR}")


if __name__ == "__main__":
    main()

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

make -s

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t reliable)
INPUT_FILE="$TMP_DIR/input.bin"
OUTPUT_FILE="$TMP_DIR/output.bin"
export INPUT_FILE

python - <<'PY'
import os
import random
path = os.environ['INPUT_FILE']
random.seed(1)
with open(path, 'wb') as f:
    f.write(os.urandom(300 * 1024))
PY

cleanup() {
  if [[ -n "${EMU_PID:-}" ]]; then kill "$EMU_PID" 2>/dev/null || true; fi
  if [[ -n "${RCV_PID:-}" ]]; then kill "$RCV_PID" 2>/dev/null || true; fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

./emulator.py --loss 0.05 --delay_ms 50 --reorder 0.05 --seed 1 &
EMU_PID=$!

./receiver_gbn --listen 10001 --peer_ip 127.0.0.1 --peer_port 10000 --out "$OUTPUT_FILE" &
RCV_PID=$!

./sender_gbn --listen 10000 --peer_ip 127.0.0.1 --peer_port 10001 \
  --in "$INPUT_FILE" --win 10 --timeout 200

wait "$RCV_PID"

if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$INPUT_FILE" "$OUTPUT_FILE"
else
  shasum -a 256 "$INPUT_FILE" "$OUTPUT_FILE"
fi

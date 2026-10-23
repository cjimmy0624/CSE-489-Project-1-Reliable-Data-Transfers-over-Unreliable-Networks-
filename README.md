# Reliable Transport (UDP) Student Project

This project asks you to implement a reliable transport layer so a sender and receiver can transfer a file correctly over an unreliable network. The network may drop, delay, or reorder packets, and your protocol must preserve data integrity and ordering while completing the transfer in a reasonable time.

You will implement two reliable protocols in this folder: Go-Back-N (GBN) and Selective Repeat (SR). The files `sender_gbn.c` / `receiver_gbn.c` / `sender_sr.c` / `receiver_sr.c` are currently basic templates that only show minimal socket usage. You must replace the basic logic with full protocol implementations.

## What You Must Implement

Your sender and receiver should provide reliable file transfer, including:

- Segmentation and reassembly: split the input file into fixed-size packets and reconstruct the file in order.
- Sequencing and ACKs: design and handle sequence numbers and acknowledgments to recover from loss and reordering.
- Reliability: implement timeouts, retransmissions, and window sliding; handle duplicate packets and duplicate ACKs.
- Clean termination: complete the transfer with a reliable FIN/FINACK handshake.

Protocol requirements:
- GBN: receiver does not buffer out-of-order data; sender uses cumulative ACKs and retransmits the entire window on timeout.
- SR: receiver buffers out-of-order data; sender manages per-packet timers and acknowledgments independently.

## Files You Should Modify

Only modify these files for the core protocol logic:

- `sender_gbn.c`
- `receiver_gbn.c`
- `sender_sr.c`
- `receiver_sr.c`

These files are currently the same as the basic examples and must be extended with full protocol behavior (sender/receiver state, windows, timers, ACK logic, FIN/FINACK, etc.).

Implementation guides:
- `docs/gbn_implementation.md`
- `docs/sr_implementation.md`

## What You Must Do

You are responsible for:

- Implementing both GBN and SR from scratch in the four protocol files.
- Verifying correctness (the received file must match the input file exactly).
- Running the provided test scripts to measure performance.
- Generating plots and writing a short report that explains the results.

Correctness comes first. Passing a single transfer is required before performance testing. However, a correct transfer alone does not prove your GBN/SR is complete or efficient. You must also evaluate performance under loss/delay/reordering and explain whether the trends make sense.

## What We Provide

- Transport framework and helpers:
  - `lib/netif.c` and `include/netif.h` provide a UDP-like socket API.
  - `lib/protocol.c` and `include/protocol.h` define packet formats and helpers.
  - `lib/crc32.c` provides CRC32 verification for packet integrity.
- Network behavior emulator:
  - `emulator.py` simulates loss, delay, and reordering.
- Reference material and scripts:
  - `scripts/test_local.sh`: local smoke test.
  - `scripts/run_reliable.py`: batch test runner.
  - `docs/scripts_guide.md`: detailed script usage.
  - `docs/gbn_implementation.md` and `docs/sr_implementation.md`: protocol guidance and
    state machine notes.
  - `docs/library_overview.md`: overview of provided libraries and headers.

## Build

From the `student/` directory:

```bash
make
```

Builds:
- `sender_gbn` / `receiver_gbn`
- `sender_sr` / `receiver_sr`
- `sender_basic` / `receiver_basic`

## Running (3 terminals)

Port wiring:
- `sender_*` listens on 10000 and connects to `receiver_*:10001`
- `receiver_*` listens on 10001 and connects to `sender_*:10000`
- `emulator.py` listens on 11000 (hidden behind `netif_*`)

Example (GBN):

Terminal 1:
```bash
./emulator.py --loss 0.05 --delay_ms 50 --reorder 0.05
```

Terminal 2:
```bash
./receiver_gbn --listen 10001 --peer_ip 127.0.0.1 --peer_port 10000 --out recv.bin
```

Terminal 3:
```bash
./sender_gbn --listen 10000 --peer_ip 127.0.0.1 --peer_port 10001 \
  --in test.bin --win 20 --timeout 200
```

SR uses `sender_sr` / `receiver_sr` and includes a window size:

```bash
./receiver_sr --listen 10001 --peer_ip 127.0.0.1 --peer_port 10000 --out recv.bin --win 20
./sender_sr --listen 10000 --peer_ip 127.0.0.1 --peer_port 10001 --in test.bin --win 20 --timeout 200
```

## Emulator Explained

The emulator sits between the sender and receiver and simulates an unreliable network. It can drop packets, delay them, or reorder them. This lets you test your reliability logic without needing a real lossy network.

How it connects:

```text
(UDP)                 (UDP)
+----------------+    +----------------+    +----------------+
|    sender_*    | -> |    emulator     | -> |   receiver_*   |
| listen:10000   |    | port:11000     |    | listen:10001   |
+----------------+    +----------------+    +----------------+
         ^                                             |
         |                                             v
         +--------------------+<-----------------------+
                              |
              ACKs from receiver (via emulator)
```

The sender and receiver do not talk directly. They both use `netif_*`, which hides the emulator behind a UDP-like API. The emulator learns the endpoints and forwards packets between them, applying loss/delay/reordering along the way.

How to start it:

```bash
./emulator.py --loss 0.05 --delay_ms 50 --reorder 0.05
```

Parameters:
- `--loss`: packet drop probability (0.0–1.0). For example, `0.1` means about 10% of packets are dropped.
- `--delay_ms`: base one-way delay in milliseconds (sender -> receiver). RTT is roughly twice this value.
- `--reorder`: packet reordering probability (some packets may arrive out of order).
- `--seed`: random seed for reproducibility (default: `1`, keep this default).
- `--rate_kbps`: link rate limit in kbps (default: `1500`, keep this default).

## Command-Line Parameters

emulator:
- `--loss`: packet loss rate (0.0–1.0)
- `--delay_ms`: base delay in ms
- `--reorder`: reordering probability
- `--seed`: RNG seed (default: `1`, keep this default)
- `--rate_kbps`: link rate limit (default: `1500`, keep this default)

sender:
- `--listen`: local listen port
- `--peer_ip`: peer IP
- `--peer_port`: peer port
- `--in`: input file path
- `--win`: window size
- `--timeout`: retransmission timeout (ms)

receiver:
- `--listen`: local listen port
- `--peer_ip`: peer IP
- `--peer_port`: peer port
- `--out`: output file path

## Testing

### 1) Local smoke test

```bash
./scripts/test_local.sh
```

This script starts the emulator, receiver, and sender, then compares output file hashes.
You pass correctness if the input and output hashes match and the receiver exits normally.
If the hashes differ, or the receiver never finishes, correctness is not achieved.

What counts as “correctness” for this project:
- The output file is identical to the input file (same SHA‑256 hash).
- The sender and receiver both terminate cleanly (no hang or crash).
- This must hold under loss/delay/reordering, not just the zero‑loss case.

### 2) Batch test matrix

GBN:
```bash
python scripts/run_reliable.py --mode gbn
python scripts/process_reliable_results.py
```

SR:
```bash
python scripts/run_reliable.py --mode sr
python scripts/process_reliable_results.py
```

SR with fast retransmit (**optional**, and only if your sender accepts `--fast_retx`; see
"Fast Retransmit (Optional)" in `docs/sr_implementation.md`). If you did not implement it,
skip this mode — the sender will reject the unknown flag:
```bash
python scripts/run_reliable.py --mode sr_fast
python scripts/process_reliable_results.py
```

Results are written under `tmp_reliable/` as JSONL files and plots.
Correctness in the batch tests is recorded as `hash_ok` in `tmp_reliable/results.jsonl`
(`1` means correct, `0` means incorrect).

## Submission

Package your submission as a **zip file** with the following structure:

```
<UBITname1>[_UBITname2...]_A2.zip
├── report.pdf
└── A2_reliable/              ← the entire project directory
    ├── sender_gbn.c
    ├── receiver_gbn.c
    ├── sender_sr.c
    ├── receiver_sr.c
    ├── Makefile
    ├── lib/
    ├── include/
    ├── scripts/
    ├── emulator.py
    ├── tmp_reliable/
    │   ├── results.jsonl     ← keep: your measured results
    │   └── plots/            ← keep: the plots used in your report
    └── ...
```

If you work in a team, include **all team members' UBITnames** in the zip file name
(e.g., `jsmith_jdoe_A2.zip`). Submit **one** zip per team.

Before zipping:
- Run `make clean` so no binaries or object files are included.
- Keep `tmp_reliable/results.jsonl` and `tmp_reliable/plots/` — we use them to check that
  the numbers and curves in your report come from real runs.
- Delete the bulky run artifacts: `recv.bin`, `tmp_reliable/input.bin`,
  `tmp_reliable/outputs/`, and `tmp_reliable/logs/`.
- Verify that `make` builds cleanly from the submitted folder.

Your code must compile and run with the provided `Makefile`, `emulator.py`, and scripts
unmodified. Do not rename files or change the command-line parameters.

### Report (`report.pdf`)

Your report should include:

- Student information: full name, UBITname, student ID, and email for every team member.
- The plots produced by `scripts/process_reliable_results.py` for GBN and SR
  (goodput and retransmission rate vs loss, delay, and window size).
- A correctness summary (e.g., a table or paragraph showing that `hash_ok=1`
  for your test runs and noting any failing scenarios).
- A short analysis of why each curve looks the way it does.
- A discussion of whether your results are reasonable and what they imply about your implementation.
- If you implemented the optional fast retransmit, include the `sr_fast` plots and compare
  them against plain SR; otherwise state that you did not attempt it. This part is optional
  and not graded.

What the plots mean:

- Goodput vs Loss/Delay/Window: how efficiently useful data is delivered as conditions change.
- Retransmission Rate vs Loss/Delay/Window: how often packets are resent under those conditions.

Examples of expected trends (you should reason about your own data):

- Higher loss typically lowers goodput and increases retransmissions.
- Larger windows can improve goodput up to a point, but may also increase retransmissions under heavy loss.
- Larger delays generally reduce goodput because ACKs return more slowly.

Use these trends to sanity‑check your implementation. If your plots look unreasonable, treat that as a sign to re‑examine your protocol logic.

## Implementation Notes

- Maximum payload size is `MAX_PAYLOAD=1000` bytes.
- CRC32 is validated on every packet; invalid packets should be dropped.
- Handle duplicates, timeouts, and retransmissions correctly.
- Use FIN/FINACK to close the transfer cleanly.
- Follow the state machines in `docs/gbn_implementation.md` and `docs/sr_implementation.md`.

## netif API Quick Guide

```c
int sock = netif_socket();
netif_bind(sock, local_port);
netif_connect(sock, peer_ip, peer_port);

netif_send(sock, buf, len);
netif_recv(sock, buf, sizeof(buf), timeout_ms);
```
#   C S E - 4 8 9 - P r o j e c t - 1 - R e l i a b l e - D a t a - T r a n s f e r s - o v e r - U n r e l i a b l e - N e t w o r k s -  
 
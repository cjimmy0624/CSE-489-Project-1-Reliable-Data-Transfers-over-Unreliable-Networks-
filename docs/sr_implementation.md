# Selective Repeat (SR) Implementation Guide

This is a high-level, student-friendly overview. It focuses on the flow and decisions, not code.

## What You Are Building

SR is like GBN, but “smarter” with out‑of‑order packets. The sender retransmits only missing packets, and the receiver can buffer packets that arrive early.

## The Big Picture (Sender)

Think of the sender like this:

1) Split the file into packets.  
2) Send packets while there is room in the window.  
3) Track which packets are ACKed individually.  
4) If a particular packet times out, resend just that one.  
5) As ACKs arrive, slide the window forward past any already‑ACKed packets.  
6) Finish with FIN/FINACK once all data is confirmed.

The key idea: “per‑packet ACKs + per‑packet retransmit.”

## The Big Picture (Receiver)

On the receiver side:

1) If the packet is within your receive window, store it.  
2) ACK it immediately (even if it’s a duplicate).  
3) When the next expected packet is present, deliver it (and any consecutive buffered packets).  
4) Packets outside the window can be dropped or ignored.  
5) Handle FIN with FINACK once everything is delivered.

## Common Pitfalls

- Not buffering out‑of‑order packets (that turns SR into GBN).  
- Forgetting to ACK duplicates.  
- Letting the receive window move incorrectly when there are gaps.  
- Mixing up “next expected” vs “highest seen.”

## Fast Retransmit (Optional)

Some SR variants add a “fast retransmit” rule: if the sender sees repeated ACKs that point to the same missing packet, it can resend that packet early, without waiting for its timeout. This is optional but can improve performance when there’s moderate loss.

At a high level:

- If you observe several ACKs that all indicate the same gap, treat it as a signal that one packet is missing.
- Retransmit the missing packet immediately.
- Keep your normal timeout‑based retransmissions as a backup.

If you implement this, be careful not to trigger unnecessary retransmissions on normal reordering.

This part is optional and not graded. If you do implement it, your `sender_sr` must accept a
`--fast_retx` flag that turns the behavior on — the test scripts pass exactly that flag when you
run `--mode sr_fast` (see `scripts/run_reliable.py` and `scripts/run_reliable_one.py`), so a
different flag name will not work. Without the flag, `sender_sr` should behave as plain SR.

## Where to Look

- Packet format and helpers: `include/protocol.h` and `lib/protocol.c`  
- Socket‑like API: `include/netif.h` and `lib/netif.c`  
- Project overview and testing instructions: `README.md`

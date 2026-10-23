# Go-Back-N (GBN) Implementation Guide

This is a high-level, student-friendly overview. It is meant to describe the flow, not give you code.

## What You Are Building

GBN is a sliding‑window reliable protocol. The sender can have several packets “in flight,” and the receiver only accepts the next expected packet. Anything out of order is ignored, and the receiver keeps sending the same ACK for the next expected sequence number.

## The Big Picture (Sender)

Think of the sender loop like this:

1) Slice the input file into fixed‑size payloads.  
2) Keep a window of packets you’ve sent but not yet confirmed.  
3) If there is room in the window, send the next packet.  
4) If an ACK comes back that moves the window forward, slide the window.  
5) If you wait too long with no progress, resend everything in the current window.  
6) When all data is confirmed, finish with a clean close (FIN/FINACK).

That’s it. The main idea is “cumulative ACKs + one timer + resend the window on timeout.”

## The Big Picture (Receiver)

The receiver is simpler:

1) If a packet is exactly the next expected sequence number, accept it and write it.  
2) Otherwise, discard it.  
3) Always ACK the next expected sequence number (even for duplicates).  
4) When you see FIN, respond with FINACK and close.

## Common Pitfalls

- Forgetting to resend the whole window on timeout.  
- Advancing the sender window too far when you get duplicate or old ACKs.  
- Not ACKing duplicates (you should keep ACKing the next expected seq).  
- Not handling FIN/FINACK reliably.

## Where to Look

- Packet format and helpers: `include/protocol.h` and `lib/protocol.c`  
- Socket‑like API: `include/netif.h` and `lib/netif.c`  
- Project overview and testing instructions: `README.md`

#!/usr/bin/env python3
import argparse
import heapq
import random
import select
import socket
import time


def now():
    return time.monotonic()


def parse_args():
    p = argparse.ArgumentParser(description="UDP unreliable channel emulator")
    p.add_argument("--port", type=int, default=11000, help="listen port")
    p.add_argument("--loss", type=float, default=0.0)
    p.add_argument("--delay_ms", type=float, default=0.0)
    p.add_argument("--reorder", type=float, default=0.0)
    p.add_argument("--rate_kbps", type=float, default=0.0, help="link rate limit (kbps), 0=unlimited")
    p.add_argument("--seed", type=int, default=1)
    return p.parse_args()


def make_sock(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    return sock


def main():
    args = parse_args()
    random.seed(args.seed)

    sock = make_sock(args.port)
    endpoints = {}
    forward = {}

    def try_pairing():
        forward.clear()
        for addr_a, info_a in list(endpoints.items()):
            for addr_b, info_b in list(endpoints.items()):
                if addr_a == addr_b:
                    continue
                if info_a["peer_port"] == addr_b[1] and info_b["peer_port"] == addr_a[1]:
                    forward[addr_a] = addr_b
                    forward[addr_b] = addr_a

    sendq = []
    seq = 0
    next_free = {}

    def schedule(dst, data, src):
        nonlocal seq
        if random.random() < args.loss:
            return
        delay = args.delay_ms / 1000.0
        if random.random() < args.reorder:
            delay += args.delay_ms / 1000.0 * 2
        deliver_at = now() + delay
        if args.rate_kbps and args.rate_kbps > 0:
            key = (src, dst)
            send_start = max(now(), next_free.get(key, 0.0))
            serialization = (len(data) * 8.0) / (args.rate_kbps * 1000.0)
            send_finish = send_start + serialization
            next_free[key] = send_finish
            deliver_at = send_finish + delay
        heapq.heappush(sendq, (deliver_at, seq, dst, data))
        seq += 1

    while True:
        timeout = None
        if sendq:
            delay = max(0.0, sendq[0][0] - now())
            timeout = delay

        rlist, _, _ = select.select([sock], [], [], timeout)
        for sock in rlist:
            data, src = sock.recvfrom(2048)
            if data.startswith(b"HELLO "):
                text = data[6:].strip().decode("ascii", errors="ignore")
                try:
                    peer_port = int(text)
                except ValueError:
                    continue
                endpoints[src] = {"peer_port": peer_port}
                try_pairing()
                continue

            dst = forward.get(src)
            if dst is not None:
                schedule(dst, data, src)

        now_ts = now()
        while sendq and sendq[0][0] <= now_ts:
            _, _, dst, data = heapq.heappop(sendq)
            sock.sendto(data, dst)


if __name__ == "__main__":
    main()

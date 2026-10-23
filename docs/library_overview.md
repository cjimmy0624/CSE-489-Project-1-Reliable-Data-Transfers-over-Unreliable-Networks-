# Library Overview

This project includes helper libraries and headers in `lib/` and `include/`. You can use these APIs to focus on protocol logic instead of raw socket plumbing.

## `lib/netif.c` and `include/netif.h`

Purpose: provides a UDP-like socket API that hides the emulator setup.

Typical use:
```c
int sock = netif_socket();
netif_bind(sock, local_port);
netif_connect(sock, peer_ip, peer_port);
netif_send(sock, buf, len);
netif_recv(sock, buf, sizeof(buf), timeout_ms);
```

Notes:
- `netif_recv` uses a timeout in milliseconds.
- The emulator is transparent; you use these functions as if it were direct UDP.

## `lib/protocol.c` and `include/protocol.h`

Purpose: defines the packet format and helper functions for encoding/decoding.

What to look for:
- Packet structures for DATA, ACK, FIN, and FINACK.
- Constants like `MAX_PAYLOAD` and header sizes.
- Helper functions to build and parse packets.

Your implementations should use the provided packet formats to stay compatible with the test scripts.

## `lib/crc32.c`

Purpose: compute and validate CRC32 checksums for packet integrity.

Notes:
- All received packets should be verified.
- Packets that fail CRC validation should be dropped.

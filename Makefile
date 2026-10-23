CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude

OBJS_COMMON = lib/netif.o lib/protocol.o lib/crc32.o

all: sender_gbn receiver_gbn sender_basic receiver_basic sender_sr receiver_sr

sender_gbn: sender_gbn.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

receiver_gbn: receiver_gbn.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

sender_basic: sender_basic.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

receiver_basic: receiver_basic.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

sender_sr: sender_sr.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

receiver_sr: receiver_sr.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o lib/*.o sender_gbn receiver_gbn sender_basic receiver_basic sender_sr receiver_sr

.PHONY: all clean

CC=gcc
CFLAGS=-g3 -Wall -std=c17
LDFLAGS=

SRC=src
BASE=$(SRC)/base

SHARED=$(SRC)/network.o $(SRC)/helpers.o $(BASE)/base.o $(SRC)/constants.o
SERVER=$(SRC)/server.o
SEND_REQUEST=$(SRC)/send_request.o

build: server send_request

server: $(SERVER) $(SHARED)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $^ $(LDFLAGS)

send_request: $(SEND_REQUEST) $(SHARED)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CLFAGS) $(EXTRAFLAGS) -c -o $@ $<

clean:
	rm -f server send_request *.o $(SRC)/*.o $(BASE)/*.o

.PHONY: build  clean

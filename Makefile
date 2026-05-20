CC=gcc
CFLAGS=-g3 -Wall -std=c17
LDFLAGS=

SRC=src
BASE=$(SRC)/base
OBJECTS=$(SRC)/server.o $(SRC)/helpers.o $(BASE)/base.o
EXE=server 

$(EXE): $(OBJECTS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CLFAGS) $(EXTRAFLAGS) -c -o $@ $<

clean:
	rm -f $(EXE) *.o $(SRC)/*.o $(BASE)/*.o

build: $(EXE)

.PHONY: build  clean

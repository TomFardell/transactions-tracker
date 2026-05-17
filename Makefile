CC=gcc
CFLAGS=-g3 -Wall -std=c17
LDFLAGS=

OBJECTS=server.o helpers.o base/base.o
EXE=server 

$(EXE): $(OBJECTS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CLFAGS) $(EXTRAFLAGS) -c -o $@ $<

clean:
	rm -f $(EXE) *.o *.out base/*.o

build: $(EXE)

.PHONY: build  clean

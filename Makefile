#CC=clang
#CFLAGS=-g -march=native -Wall -std=gnu99 -O2 -flto -B/home/abyrd/svn/binutils/build/gold/ld-new -use-gold-plugin
CC=gcc
CFLAGS=-g -march=native -Wall -std=gnu99 -O2
LIBS=-lzmq -lczmq
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
BINS=workerrrr brrrroker client lookup-console

all: $(BINS)

%.o: %.c
	$(CC) $^ -c $(CFLAGS) -o $@

brrrroker: broker.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 
		
workerrrr: bitset.o qstring.o router.o tdata.o util.o worker.o bitset.o hashgrid.o geometry.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

client: bitset.o qstring.o router.o tdata.o util.o client.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

lookup-console: tdata.o util.o lookup-console.o trie.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

clean:
	rm -f *.o *~ core $(BINS)



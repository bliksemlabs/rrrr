CC=clang
CFLAGS=-g -march=native -Wall -std=gnu99 #-O2 -flto -B/home/abyrd/svn/binutils/build/gold/ld-new -use-gold-plugin
#CC=gcc
#CFLAGS=-g -march=native -Wall -std=gnu99 #-O2
LIBS=-lzmq -lczmq -lm 
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
BINS=workerrrr brrrroker client lookup-console hashgrid testerrrr

all: $(BINS)

%.o: %.c
	$(CC) $^ -c $(CFLAGS) -o $@

hashgrid: hashgrid.o geometry.o tdata.o util.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

brrrroker: broker.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 
		
workerrrr: bitset.o qstring.o router.o tdata.o util.o worker.o bitset.o 
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

testerrrr: bitset.o qstring.o router.o tdata.o util.o unittest.o bitset.o 
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

client: bitset.o qstring.o router.o tdata.o util.o client.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

lookup-console: tdata.o util.o lookup-console.o trie.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

clean:
	rm -f *.o *~ core $(BINS)



# CC      := gcc -std=gnu99
CC      := clang
CFLAGS  := -g -march=native -Wall -Wno-unused-function -Wno-unused-variable -O0  `sdl-config --cflags` # -flto -B/home/abyrd/svn/binutils/build/gold/ld-new -use-gold-plugin
LIBS    := -lzmq -lczmq -lm -lwebsockets -lprotobuf-c
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
BINS    := webworkerrrr workerrrr brrrroker client lookup-console hashgrid testerrrr explorerrrr rrrrealtime

#CC=gcc
#CFLAGS=-g -march=native -Wall -std=gnu99 #-O2

all: $(BINS)

%.o: %.c
	$(CC) $^ -c $(CFLAGS) -o $@

hashgrid: hashgrid.o geometry.o tdata.o util.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

brrrroker: broker.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 
		
workerrrr: worker.o bitset.o qstring.o router.o tdata.o util.o bitset.o json.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

webworkerrrr: worker-web.o parse.o bitset.o qstring.o router.o tdata.o util.o bitset.o json.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

testerrrr: tester.o parse.o bitset.o qstring.o router.o tdata.o util.o bitset.o json.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

explorerrrr: explorer.o bitset.o qstring.o router.o tdata.o util.o bitset.o json.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

rrrrealtime: realtime.o gtfs-realtime.pb-c.o radixtree.o tdata.o util.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

client: client.o bitset.o qstring.o router.o tdata.o util.o json.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

lookup-console: tdata.o util.o lookup-console.o trie.o gtfs-realtime.pb-c.o radixtree.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

clean:
	rm -f *.o *~ core $(BINS)



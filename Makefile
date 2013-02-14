CC=gcc
CFLAGS=-g -Wall -std=gnu99 -o2
LIBS=-lzmq -lczmq
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
BINS=workerrrr brrrroker client

all: $(BINS)

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

brrrroker: broker.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 
		
workerrrr: qstring.o router.o transitdata.o util.o worker.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

client: qstring.o router.o transitdata.o util.o client.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

clean:
	rm -f *.o *~ core $(BINS)



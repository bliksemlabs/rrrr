CC=gcc
CFLAGS=-g -Wall -std=gnu99 -o2
LIBS=-lfcgi -lzmq -lczmq
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)

all: loadbalancer rrrr test

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

loadbalancer: loadbalancer.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 
		
rrrr: qstring.o router.o transitdata.o util.o rrrr.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

test: qstring.o router.o transitdata.o util.o test.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ 

clean:
	rm -f *.o *~ core rrrr



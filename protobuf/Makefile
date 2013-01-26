CC=gcc
CFLAGS=-g -Wall -std=c99 -o3
LIBS=-lprotobuf-c -lz
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=pbf

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LIBS) -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

test: $(SOURCES)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

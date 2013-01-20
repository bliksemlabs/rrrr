CC=gcc
CFLAGS=-g -Wall -o3
LDFLAGS=
SOURCES=rrrr.c qstring.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=rrrr

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -l fcgi -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	
clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

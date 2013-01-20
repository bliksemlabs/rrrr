CC=gcc
CFLAGS=-g -Wall -o3
LDFLAGS=
SOURCES=craptor.c qstring.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=craptor

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -l fcgi -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
	
clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

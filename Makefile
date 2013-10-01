CC      := clang
CFLAGS  := -g -march=native -Wall -Wno-unused -O3 -D_GNU_SOURCE # -flto -B/home/abyrd/svn/binutils/build/gold/ld-new -use-gold-plugin
LIBS    := -lzmq -lczmq -lm -lwebsockets -lprotobuf-c
SOURCES := $(filter-out rrrrealtime-viz.c,$(wildcard *.c))
OBJECTS := $(SOURCES:.c=.o)
BINS    := workerrrr-web workerrrr brrrroker client lookup-console hashgrid testerrrr explorerrrr rrrrealtime otp_api otp_client struct_test
HEADERS := $(wildcard *.h)

BIN_BASES   := $(subst rrrr,r,$(BINS))
BIN_SOURCES := $(BIN_BASES:=.c)
BIN_OBJECTS := $(BIN_BASES:=.o)
LIB_SOURCES := $(filter-out $(BIN_SOURCES),$(SOURCES))
LIB_OBJECTS := $(filter-out $(BIN_OBJECTS),$(OBJECTS))

.PHONY: clean show check all 

all: $(BINS)

# make an archived static library to link into all executables
librrrr.a: $(LIB_OBJECTS)
	ar crsT librrrr.a $(LIB_OBJECTS)

# recompile everything if any headers change
%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $*.c -o $*.o

# re-expand variables in subsequent rules
.SECONDEXPANSION:

# each binary depends on its own .o file and the library
$(BINS): $$(subst rrrr,r,$$@).o librrrr.a
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

# rrrrealtime-viz is exceptional and compiled separately because it uses libSDL, libGL, and libshp

realtime-viz.o: realtime-viz.c
	$(CC) -c $(CFLAGS) $(shell sdl-config --cflags) $^ -o $@

rrrrealtime-viz: realtime-viz.o $(OTHER_OBJECTS)
	$(CC) $(CFLAGS) $(shell sdl-config --cflags) $^ $(OTHER_OBJECTS) $(LIBS) -lSDL -lGL -lshp -o $@

clean:
	rm -f *.o *.d *.a *~ core $(BINS)

show:
	# $(SOURCES)
	# $(OBJECTS)
	# $(BIN_BASES)
	# $(BIN_OBJECTS)
	# $(OTHER_OBJECTS)

check:
	check


CC      := clang
CFLAGS  := -g -march=native -Wall -Wno-unused -O0 -D_GNU_SOURCE # -flto -B/home/abyrd/svn/binutils/build/gold/ld-new -use-gold-plugin
LIBS    := -lzmq -lczmq -lm -lwebsockets -lprotobuf-c
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
BINS    := workerrrr-web workerrrr brrrroker client lookup-console testerrrr explorerrrr rrrrealtime rrrrealtime-viz otp_api otp_client struct_test
HEADERS := $(wildcard *.h)

BIN_BASES   := $(subst rrrr,r,$(BINS))
BIN_SOURCES := $(BIN_BASES:=.c)
BIN_OBJECTS := $(BIN_BASES:=.o)
LIB_SOURCES := $(filter-out $(BIN_SOURCES),$(SOURCES))
LIB_OBJECTS := $(filter-out $(BIN_OBJECTS),$(OBJECTS))
LIB_NAME    := librrrr.a

.PHONY: clean show check all 

all: $(BINS)

# make an archived static library to link into all executables
librrrr.a: $(LIB_OBJECTS)
	ar crsT $(LIB_NAME) $(LIB_OBJECTS)

# recompile everything if any headers change
%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $*.c -o $*.o

# re-expand variables in subsequent rules
.SECONDEXPANSION:

# each binary depends on its own .o file and the library
$(filter-out rrrrealtime-viz,$(BINS)): $$(subst rrrr,r,$$@).o $(LIB_NAME)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

# rrrrealtime-viz is exceptional and compiled separately because it uses libSDL, libGL, and libshp

realtime-viz.o: realtime-viz.c
	$(CC) -c $(CFLAGS) $(shell sdl-config --cflags) $^ -o $@

rrrrealtime-viz: realtime-viz.o $(LIB_NAME)
	$(CC) $(CFLAGS) $(shell sdl-config --cflags) $^ $(LIBS) -lSDL -lGL -lshp -o $@

clean:
	rm -f *.o *.d *.a *~ core $(BINS)
	rm -f tests/*.o tests/*~ run_tests

show:
	# $(SOURCES)
	# $(OBJECTS)
	# $(BIN_BASES)
	# $(BIN_OBJECTS)
	# $(OTHER_OBJECTS)

# TESTS using http://check.sourceforge.net/

TEST_SOURCES := $(wildcard tests/*.c)
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)
TEST_LIBS    := -lcheck -lprotobuf-c -lm

check: run_tests
	./run_tests

run_tests: $(TEST_OBJECTS) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ $(TEST_LIBS) -o run_tests



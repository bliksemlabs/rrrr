PCC   = cython
GCC   = clang
FLAGS = -shared -fPIC -fno-strict-aliasing -fwrapv -O3 -Wall -Wextra -L. -I/usr/include/python2.7 -I.. -lrrrr -lprotobuf-c #The last flag adds higher folder to search path

PYXS = $(wildcard *.pyx)
OBJS = $(PYXS:.pyx=.so)

TMP = temp_file.code

all: upper $(OBJS)

%.so: %.pyx Makefile
	export LD_LIBRARY_PATH=.:/usr/lib/
	$(PCC) -o $(TMP) $<
	$(GCC) -x c $(FLAGS) -o $@ $(TMP)
	rm -f $(TMP)
	strip $@

upper:
	make -C ../

clean:
	rm -f $(OBJS) $(TMP)
	make clean -C ../


.PHONY: all clean

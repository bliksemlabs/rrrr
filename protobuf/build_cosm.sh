#!/bin/bash
# ftruncate etc. aren't in c99, they are gnu extensions
gcc -g cosm.c pbf.o fileformat.pb-c.o osmformat.pb-c.o -lz -lprotobuf-c -std=gnu99 -O3 -o cosm


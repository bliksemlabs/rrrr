#!/bin/bash
# -lfcgi must be after source files
gcc -Wall -g *.c -o craptor -l fcgi

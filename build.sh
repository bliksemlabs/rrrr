#!/bin/bash
# -lfcgi must be after source files
gcc -g *.c -o craptor -l fcgi

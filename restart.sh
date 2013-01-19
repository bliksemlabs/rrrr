#!/bin/bash
# -lfcgi must be after source files
NUM_HANDLERS=10
killall craptor
cgi-fcgi -start -connect localhost:9000  ./craptor $NUM_HANDLERS

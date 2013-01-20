#!/bin/bash
# -lfcgi must be after source files
NUM_HANDLERS=4
killall craptor
cgi-fcgi -start -connect /tmp/fastcgi.socket ./craptor $NUM_HANDLERS

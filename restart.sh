#!/bin/bash
NUM_HANDLERS=8
killall rrrr
cgi-fcgi -start -connect /tmp/fastcgi.socket ./rrrr $NUM_HANDLERS
chmod a+rw /tmp/fastcgi.socket

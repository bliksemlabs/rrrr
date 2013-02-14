#!/bin/bash
echo killing old processes
killall brrrroker
killall workerrrr
#sleep 1
echo starting new processes
./brrrroker&
# 8 workers is not significantly faster than 4 on a 4-core machine (confirmed twice)
for i in {1..4} ; do ./workerrrr & done
#sleep 1
./client 1 1
# old fcgi:
# cgi-fcgi -start -connect /tmp/fastcgi.socket ./rrrr $NUM_HANDLERS
# chmod a+rw /tmp/fastcgi.socket


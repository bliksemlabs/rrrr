#!/bin/bash
echo killing old processes
killall loadbalancer
killall rrrr
sleep 1
echo starting new processes
./loadbalancer&
# 8 workers is not significantly faster than 4 on a 4-core machine (confirmed twice)
for i in {1..4} ; do ./rrrr & done
sleep 1
echo done

# old fcgi:
# cgi-fcgi -start -connect /tmp/fastcgi.socket ./rrrr $NUM_HANDLERS
# chmod a+rw /tmp/fastcgi.socket


#!/bin/bash

./haltes ns_stops.txt
read -r FROM < /tmp/stopid
echo $FROM
./haltes ns_stops.txt
read -r TO < /tmp/stopid
echo $TO
./client id $FROM $TO | python ./resolver.py ./nl.gtfsdb


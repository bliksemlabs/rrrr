#!/bin/bash

./haltes stops
read -r FROM < /tmp/stopid
echo $FROM
./haltes stops
read -r TO < /tmp/stopid
echo $TO
./client id $FROM $TO | python ./resolver.py ./nl.gtfsdb


#!/bin/bash
echo "From:"
../bliksem-geocoder/trie/haltes stops
FROM=`cat /tmp/stopid`
echo
echo "To:"
../bliksem-geocoder/trie/haltes stops
TO=`cat /tmp/stopid`

./client id $FROM $TO | ./resolver.py ../data/nl.gtfsdb

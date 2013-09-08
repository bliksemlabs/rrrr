all: unittest

unittest: unittest1 unittest2

unittest1: tmp/timetable.dat 
	cd tmp ; ../../../testerrrr --depart -D 2014-01-01T00:01:00 --from-idx 0 --to-idx 1 -T timetable.dat -g ../gtfs-rt/3e.pb

unittest2: tmp/timetable.dat 
	../../testerrrr --depart -D 2014-01-01T00:01:00 --from-idx 0 --to-idx 1 --banned-route-trip 0,0 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


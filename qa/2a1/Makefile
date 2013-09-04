all: unittest

unittest: tmp/timetable.dat unittest1 unittest2 

unittest1:
	../../testerrrr --depart -D 2014-01-01T00:01:00 --from 0 --to 3 -T tmp/timetable.dat

unittest2:
	../../testerrrr --depart -D 2014-01-01T00:03:00 --from 0 --to 3 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


all: unittest

unittest: tmp/timetable.dat
	../../testerrrr --depart -D 2014-01-01T00:01:00 --from 0 --to 2 --banned-stops 1 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


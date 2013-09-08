all: unittest

unittest: tmp/timetable.dat unittest1 unittest2 unittest3 unittest4 unittest5

unittest1:
	../../testerrrr --depart -D 2014-01-01T00:02:00 --from 0 --to 1 -T tmp/timetable.dat

unittest2:
	../../testerrrr --from 0 --to 1 --arrive -D 2014-01-01T00:03:00 tmp/timetable.dat

unittest3:
	../../testerrrr --depart -D 2014-01-02T00:03:00 --from 0 --to 1 -T tmp/timetable.dat

unittest4:
	../../testerrrr --from 0 --to 1 --arrive -D 2014-01-02T00:04:00 -T tmp/timetable.dat

unittest5:
	../../testerrrr --depart -D 2014-01-03T00:03:00 --from 0 --to 1 -T tmp/timetable.dat

unittest6:
	../../testerrrr --from 0 --to 1 --arrive -D 2014-01-03T00:04:00 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


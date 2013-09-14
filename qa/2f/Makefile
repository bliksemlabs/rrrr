all: unittest

unittest: tmp/timetable.dat
	../../testerrrr --depart --start-trip-idx 0,0 --from-idx 0 --to-idx 2 -D 2014-01-01T00:02:00 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


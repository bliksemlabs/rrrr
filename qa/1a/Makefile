all: unittest

unittest: tmp/timetable.dat
	../../testerrrr -d -m 20140101T00:00:00 -f 0 -t 5 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


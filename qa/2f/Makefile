all: unittest

unittest: tmp/timetable.dat
	../../testerrrr --depart --from-idx-trip 0 --from-idx 0 --to-idx 1 -T tmp/timetable.dat

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


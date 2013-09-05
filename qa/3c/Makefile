all: unittest

unittest: tmp/timetable.dat
	cd tmp ; ../../../testerrrr --depart -D 2014-01-01T00:04:00 --from 1 --to 2 -T timetable.dat -g ../gtfs-rt/3c.pb

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


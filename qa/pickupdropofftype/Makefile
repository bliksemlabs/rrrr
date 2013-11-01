all: unittest

unittest: tmp/timetable.dat
	../../testerrrr --depart -D 2014-01-01T00:00:00 --from-idx 0 --to-idx 3  -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


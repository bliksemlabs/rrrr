all: unittest

unittest: tmp/timetable.dat unittest1 unittest2 unittest3 unittest4 unittest5 unittest6

unittest1:
	../../testerrrr --depart -D 2014-01-01T00:02:00 --from-idx 0 --to-idx 1 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest2:
	../../testerrrr --from-idx 0 --to-idx 1 --arrive -D 2014-01-01T00:03:00 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest3:
	../../testerrrr --depart -D 2014-01-02T00:03:00 --from-idx 0 --to-idx 1 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest4:
	../../testerrrr --from-idx 0 --to-idx 1 --arrive -D 2014-01-02T00:04:00 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest5:
	../../testerrrr --depart -D 2014-01-03T00:03:00 --from-idx 0 --to-idx 1 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest6:
	../../testerrrr --from-idx 0 --to-idx 1 --arrive -D 2014-01-03T00:04:00 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


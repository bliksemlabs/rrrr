all: unittest

unittest: tmp/timetable.dat unittest3 unittest4 unittest5 

unittest3:
	../../testerrrr --depart -D 2014-01-01T00:01:00 --from-idx 0 --to-idx 3 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest4:
	../../testerrrr --depart -D 2014-01-01T00:02:00 --from-idx 0 --to-idx 3 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"

unittest5:
	../../testerrrr --from-idx 0 --to-idx 3 --arrive -D 2014-01-01T00:11:00 -T tmp/timetable.dat \
	| tee tmp/$@.txt ; echo ; diff --suppress-common-lines -w -W 200 -y expected/$@.txt tmp/$@.txt && echo -e "PASSED\n\n"


tmp/gtfsdb: gtfs/*
	mkdir -p tmp
	../../gtfsdb.py gtfs tmp/gtfsdb

tmp/timetable.dat: tmp/gtfsdb
	cd tmp ; ../../../timetable.py gtfsdb

clean:
	rm -r tmp


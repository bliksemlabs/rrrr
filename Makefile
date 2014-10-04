all:
	protoc-c --c_out=. gtfs-realtime.proto
	gcc -c -Wall -ansi -pedantic list.c
	gcc -c -Wall -ansi -pedantic bitset.c
	gcc -c -Wall -ansi -pedantic geometry.c
	gcc -c -Wall -ansi -pedantic radixtree.c
	gcc -c -Wall -ansi -pedantic tdata_validation.c
	gcc -c -Wall -ansi -pedantic tdata.c
	gcc -o cli -Wall -ansi -pedantic cli.c stubs.c
	gcc -c -Wall -ansi -pedantic tdata_realtime_alerts.c
	gcc -c -Wall -ansi -pedantic tdata_realtime.c
	gcc -c -Wall -ansi -pedantic router_request.c

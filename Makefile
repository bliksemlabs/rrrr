CC=clang

debug:
	$(CC) -DRRRR_FAKE_REALTIME -DRRRR_VALGRIND -DRRRR_BITSET_64 -Wextra -Wall -ansi -pedantic -DRRRR_DEBUG -DRRRR_INFO -DRRRR_TDATA -ggdb -O0 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v3_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c router_dump.c hashgrid.c
	$(CC) -DRRRR_FAKE_REALTIME -DRRRR_VALGRIND -DRRRR_BITSET_64 -Wextra -Wall -ansi -pedantic -DRRRR_DEBUG -DRRRR_INFO -DRRRR_TDATA -ggdb -O0 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v3_mmap.c radixtree.c gtfs-realtime.pb-c.c geometry.c router_dump.c hashgrid.c

valgrind:
	$(CC) -DRRRR_FAKE_REALTIME -DRRRR_VALGRIND -DRRRR_BITSET_128 -DNDEBUG -O0 -ggdb3 -Wextra -Wall -std=c99 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v3_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c hashgrid.c

prod:
	$(CC) -DRRRR_BITSET_128 -DNDEBUG -O3 -Wextra -Wall -std=c99 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v3_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c hashgrid.c

all:
	protoc-c --c_out=. gtfs-realtime.proto
	$(CC) -c -Wextra -Wall -ansi -pedantic util.c
	$(CC) -c -Wextra -Wall -ansi -pedantic list.c
	$(CC) -c -Wextra -Wall -ansi -pedantic bitset.c
	$(CC) -DRRRR_BITSET_64  -c -Wextra -Wall -ansi -pedantic bitset.c
	$(CC) -DRRRR_BITSET_128 -c -Wextra -Wall -std=c99 bitset.c
	$(CC) -c -Wextra -Wall -ansi -pedantic geometry.c
	$(CC) -c -Wextra -Wall -ansi -pedantic radixtree.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_validation.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_io_v3_dynamic.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_io_v3_mmap.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_realtime_alerts.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_realtime_expanded.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router_request.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router_dump.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router_result.c
	# $(CC) -o cli -Wextra -Wall -ansi -pedantic cli.c stubs.c
	$(CC) -lm -lprotobuf-c -o cli -Wextra -Wall -ansi -pedantic cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_alerts.c tdata_realtime_expanded.c tdata_io_v3_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c hashgrid.c

clean:
	rm *.o gtfs-realtime.pb-c.c gtfs-realtime.pb-c.h



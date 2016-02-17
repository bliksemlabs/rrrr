CC=clang

debug:
	$(CC) -DRRRR_STRICT -DRRRR_TDATA_IO_DYNAMIC -DRRRR_FAKE_REALTIME -DRRRR_VALGRIND -DRRRR_BITSET_64 -Wextra -Wall -ansi -pedantic -DRRRR_DEBUG -DRRRR_INFO -DRRRR_TDATA -ggdb -O0 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v4_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c router_dump.c hashgrid.c plan_render_text.c polyline.c json.c plan_render_otp.c api.c set.c string_pool.c hashgrid_street_network.c street_network.c
	$(CC) -DRRRR_STRICT -DRRRR_TDATA_IO_MMAP -DRRRR_FAKE_REALTIME -DRRRR_VALGRIND -DRRRR_BITSET_64 -Wextra -Wall -ansi -pedantic -DRRRR_DEBUG -DRRRR_INFO -DRRRR_TDATA -ggdb -O0 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v4_mmap.c radixtree.c gtfs-realtime.pb-c.c geometry.c router_dump.c hashgrid.c plan_render_text.c polyline.c json.c plan_render_otp.c api.c set.c string_pool.c hashgrid_street_network.c street_network.c

valgrind:
	$(CC) -I/usr/local/include -L/usr/local/lib -DRRRR_STRICT -DRRRR_FAKE_REALTIME -DRRRR_VALGRIND -DRRRR_BITSET_128 -DNDEBUG -O0 -ggdb3 -Wextra -Wall -std=c99 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v4_dynamic.c tdata_io_v4_mmap.c radixtree.c gtfs-realtime.pb-c.c geometry.c hashgrid.c plan_render_text.c polyline.c json.c plan_render_otp.c api.c set.c string_pool.c hashgrid_street_network.c street_network.c

prod:
	$(CC) -DRRRR_BITSET_128 -DNDEBUG -O3 -Wextra -Wall -std=c99 -lm -lprotobuf-c -o cli cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_realtime_expanded.c tdata_realtime_alerts.c tdata_io_v4_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c hashgrid.c plan_render_text.c polyline.c json.c plan_render_otp.c api.c set.c string_pool.c hashgrid_street_network.c street_network.c api.c plan.c

ioscli:
	$(CC) -isysroot /var/sdks/Latest.sdk -DRRRR_TDATA_IO_MMAP -DRRRR_BITSET_64 -DNDEBUG -O2 -Wextra -Wall -std=c99 -lm -o cli router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_io_v4_mmap.c radixtree.c geometry.c hashgrid.c cli.c plan_render_text.c api.c set.c string_pool.c hashgrid_street_network.c street_network.c

ios:
	$(CC) -isysroot /var/sdks/Latest.sdk -DRRRR_TDATA_IO_MMAP -DRRRR_BITSET_64 -DNDEBUG -O2 -Wextra -Wall -std=c99 -c router.c tdata.c tdata_validation.c bitset.c router_request.c router_result.c util.c tdata_io_v4_mmap.c radixtree.c geometry.c hashgrid.c api.c set.c string_pool.c hashgrid_street_network.c
	libtool -static -o ../librrrr.a router.o tdata.o tdata_validation.o bitset.o router_request.o router_result.o util.o tdata_io_v4_mmap.o radixtree.o geometry.o hashgrid.o api.o set.o string_pool.o hashgrid_street_network.o street_network.o

all:
	protoc-c --c_out=. gtfs-realtime.proto
	$(CC) -c -Wextra -Wall -ansi -pedantic set.c
	$(CC) -c -Wextra -Wall -ansi -pedantic json.c
	$(CC) -c -Wextra -Wall -ansi -pedantic string_pool.c
	$(CC) -c -Wextra -Wall -ansi -pedantic polyline.c
	$(CC) -c -Wextra -Wall -ansi -pedantic util.c
	$(CC) -c -Wextra -Wall -ansi -pedantic linkedlist.c
	$(CC) -c -Wextra -Wall -ansi -pedantic bitset.c
	$(CC) -DRRRR_BITSET_64  -c -Wextra -Wall -ansi -pedantic bitset.c
	$(CC) -DRRRR_BITSET_128 -c -Wextra -Wall -std=c99 bitset.c
	$(CC) -c -Wextra -Wall -ansi -pedantic geometry.c
	$(CC) -c -Wextra -Wall -ansi -pedantic radixtree.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_validation.c
	$(CC) -DRRRR_TDATA_IO_DYNAMIC -c -Wextra -Wall -ansi -pedantic tdata_io_v4_dynamic.c
	$(CC) -DRRRR_TDATA_IO_MMAP -c -Wextra -Wall -ansi -pedantic tdata_io_v4_mmap.c
	$(CC) -DRRRR_STRICT -c -Wextra -Wall -ansi -pedantic tdata.c
	$(CC) -c -Wextra -Wall -ansi -pedantic street_network.c
	$(CC) -c -Wextra -Wall -ansi -pedantic hashgrid_street_network.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_realtime_alerts.c
	$(CC) -c -Wextra -Wall -ansi -pedantic tdata_realtime_expanded.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router_request.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router_dump.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router.c
	$(CC) -c -Wextra -Wall -ansi -pedantic router_result.c
	$(CC) -c -Wextra -Wall -ansi -pedantic plan_render_text.c
	$(CC) -c -Wextra -Wall -ansi -pedantic plan_render_otp.c
	$(CC) -c -Wextra -Wall -ansi -pedantic api.c
	# $(CC) -o cli -Wextra -Wall -ansi -pedantic cli.c stubs.c
	$(CC) -lm -lprotobuf-c -o cli -Wextra -Wall -ansi -pedantic cli.c router.c tdata.c tdata_validation.c bitset.c router_request.c street_network.c hashgrid_street_network.c router_result.c util.c tdata_realtime_alerts.c tdata_realtime_expanded.c tdata_io_v4_dynamic.c radixtree.c gtfs-realtime.pb-c.c geometry.c hashgrid.c plan_render_text.c polyline.c api.c set.c string_pool.c

CC=gcc

all:
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_journey_pattern_points ../journey_pattern_points.c ../stop_points.c ../string_pool.c test_journey_pattern_points.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_routes ../routes.c ../lines.c ../physical_modes.c ../operators.c ../string_pool.c test_routes.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_lines ../lines.c ../physical_modes.c ../operators.c ../string_pool.c test_lines.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_commercial_modes ../commercial_modes.c ../string_pool.c test_commercial_modes.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_physical_modes ../physical_modes.c ../string_pool.c test_physical_modes.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_stop_areas ../stop_areas.c ../string_pool.c test_stop_areas.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_stop_points ../stop_points.c ../string_pool.c test_stop_points.c
	$(CC) -O0 -ggdb3 -Wextra -Wall -ansi -pedantic -I.. -o test_operators ../operators.c ../string_pool.c test_operators.c


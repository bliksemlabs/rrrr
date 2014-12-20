RRRR Rapid Real-time Routing
==============================

Introduction
------------

RRRR (usually pronounced R4) is a C-language implementation of the RAPTOR public transit routing algorithm. It is the core routing component of the Bliksem journey planner and passenger information system. The goal of this project is to generate sets of Pareto-optimal itineraries over large geographic areas (e.g. BeNeLux or all of Europe), improving on the resource consumption and complexity of existing more flexible alternatives. The system should supports real-time vehicle/trip updates reflected in trip plans and is capable of running directly on a mobile device with no Internet connection.

Multiple RRRR processes running on the same machine map the same read-only data file into their private address space. This file contains a structured and indexed representation of transit timetables and other information from a source such as a GTFS feed. Additional handler processes should only increase physical memory consumption by the amount needed for search state (roughly 16 * num_stops * max_transfers bytes, on the order of a few megabytes).

Each worker is a separate process and keeps a permanent scratch buffer that is reused from one request to the next, so no dynamic memory allocation is performed in inner loops. Transit stops are the only exact locations considered. Latitude and longitude location searches map to the closest locations. On-street searches were not be handled in the first phase of development, it is one of our next development targets.


Dependencies
------------

1. **libprotobuf-c**:
For decoding incoming realtime messages. https://code.google.com/p/protobuf-c/downloads/list, Ubuntu packages libprotobuf-c0 and libprotobuf-c0-dev.

1. **gcc** or **clang**:
clang provides very good error messages and warnings. At present time gcc provides the best speed when using -O3 -flto when link-time optimization is available. https://gcc.gnu.org/ http://clang.llvm.org/

1. **check**:
a unit testing framework for c. http://check.sourceforge.net/.


Building transit data
---------------------

The ansi branch currently does not include any build scripts. Your best bet for generating timetable v3 outside the realm of Bliksem Integration will be timetable.py from the realtime_expanded2 branch.
First, run `python gtfsdb.py input.gtfs.zip output.gtfsdb` to load your GTFS feed into an SQLite database.
Next, run `python transfers.py output.gtfsdb` to add distance-based transfers to the transfers table in the database.
Finally, run `python timetable.py output.gtfsdb` to create the timetable file `timetable.dat` based on that GTFS database.
If you just want to experiment with the code download: http://1313.nl/timetable.dat

Coding conventions
------------------

As the ansi branch name suggests, we build c89 code, for being fully compatible with virtually any toolchain out there. Therefore use specific types from stdint.h for cross-platform compatibility.
Absolute times are stored in uint64 and referenced from the Epoch. The absolute time are always stored with DST disabled this as DST is defined at serviceday instead of the usual 3 am.
Times in schedules are uint16 and referenced from midnight. 2**16 / 60 / 60 is only 18 hours, but by right-shifting all times one bit we get 36 hours (1.5 days) at 2 second resolution.
When the result of a function is an array, the function should return a pointer to the array and store the number of elements in an indirected parameter (rather than the other way around).
The data file is mostly a column store.

Building and starting up RRRR
-----------------------------

Using cmake (prefered):
```bash
rm -r build ; mkdir build && cd build && cmake .. && make
```

Using make:
```bash
make clean && make && ./cli
```

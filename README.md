RRRR Rapid Round-based Routing
==============================

Introduction
------------

RRRR is a C-language implementation of the RAPTOR public transit routing algorithm. It is the core routing component of the Bliksem journey planner and passenger information system. The goal of this project is to generate sets of Pareto-optimal itineraries over large geographic areas (e.g. BeNeLux or all of Europe), improving on the resource consumption and complexity of existing more flexible alternatives. The system should eventually support real-time vehicle/trip updates reflected in trip plans and be capable of running directly on a mobile device with no Internet connection.

Multiple RRRR processes running on the same machine map the same read-only data file into their private address space. This file contains a structured and indexed representation of transit timetables and other information from a GTFS feed. Additional handler processes should only increase physical memory consumption by the amount needed for search state (roughly 16 * num_stops * max_transfers bytes, on the order of a few megabytes). Eventually the real-time updater process will probably also use memory-mapped files for interprocess communication.

Each worker is a separate process and keeps a permanent scratch buffer that is reused from one request to the next, so no dynamic memory allocation is performed in inner loops. Transit stops are the only locations considered. On-street searches will not be handled in the first phase of development. Eventually we will probably use protocol buffers over a 0MQ fan-out pattern to distribute real-time updates. This is basically standard GTFS-RT over a message passing system instead of HTTP pull.

It looks like in may be possible to keep memory consumption for a Portland, Oregon-sized system under 10MiB. Full Netherlands coverage should be possible in about 20MiB.

Dependencies
------------

1. **zeromq** and **libczmq**:
Message passing / concurrency framework used by RRRR for load balancing and scaling.

1. **libprotobuf-c**:
For decoding incoming realtime messages. https://code.google.com/p/protobuf-c/downloads/list, Ubuntu packages libprotobuf-c0 and libprotobuf-c0-dev.

1. **libwebsockets**:
For receiving incremental realtime messages. https://github.com/warmcat/libwebsockets.

1. **shapelib**:
For visualizing realtime messages in OpenGL using a map of choice. http://download.osgeo.org/shapelib/.

1. **libsdl**:
For visualizing realtime messages in OpenGL via een SDL surface. http://libsdl.org/download-1.2.php.

1. **gcc** or **clang**:
clang provides very good error messages and warnings. RRRR benefits greatly from -O2 and link-time optimization. http://clang.llvm.org/.

1. **check**:
a unit testing framework for c. http://check.sourceforge.net/.


Building transit data
---------------------

Download a GTFS feed for your favorite transit agency. We typically work with http://gtfs.ovapi.nl/gtfs-nl.zip.
We used to depend on Graphserver for its gtfsdb Python class which loads a GTFS feed into Sqlite and 
provides query methods. We have now copied our customized gtfsdb class into the RRRR repository.
First, run `python gtfsdb.py input.gtfs.zip output.gtfsdb` to load your GTFS feed into an SQLite database.
Next, run `python transfers.py output.gtfsdb` to add distance-based transfers to the transfers table in the database.
Finally, run `python timetable.py output.gtfsdb` to create the timetable file `timetable.dat` based on that GTFS database.


Coding conventions
-----------------------------
Use specific types from stdint.h for cross-platform compatibility.
Absolute times are stored in uint64 and referenced from the Epoch. The absolute time are always stored with DST disabled this as DST is defined at serviceday instead of the usual 3 am.
Times in schedules are uint16 and referenced from midnight. 2**16 / 60 / 60 is only 18 hours, but by right-shifting all times one bit we get 36 hours (1.5 days) at 2 second resolution.
When the result of a function is an array, the function should return a pointer to the array and store the number of elements in an indirected parameter (rather than the other way around).
The data file is mostly a column store.


Building and starting up RRRR
-----------------------------

```bash
~/git/rrrr$ make clean && make && ./rrrr.sh
```

you should see make output followed by the broker and workers restarting:

```
killing old processes
rrrr[31023]: broker terminating
rrrr[31024]: worker terminating
rrrr[31025]: worker terminating
rrrr[31026]: worker terminating
rrrr[31027]: worker terminating

starting new processes
rrrr[31109]: broker starting up
rrrr[31111]: worker starting up
rrrr[31110]: worker starting up
rrrr[31112]: worker starting up
rrrr[31113]: worker starting up
rrrr[31111]: worker sent ready message to load balancer
rrrr[31110]: worker sent ready message to load balancer
rrrr[31112]: worker sent ready message to load balancer
rrrr[31113]: worker sent ready message to load balancer
rrrr[31114]: test client starting
rrrr[31114]: test client number of requests: 1
rrrr[31114]: test client concurrency: 1
rrrr[31114]: test client thread will send 1 requests
rrrr[31114]: test client received response: OK
rrrr[31114]: test client thread terminating
rrrr[31114]: 1 threads, 1 total requests, 0.036565 sec total time (27.348557 req/sec)
````

Then you should be able to run the test client:

```
~/git/rrrr$ ./client rand 1000 4

rrrr[31144]: test client starting
rrrr[31144]: test client number of requests: 1000
rrrr[31144]: test client concurrency: 4
rrrr[31144]: test client thread will send 250 requests
rrrr[31144]: test client thread will send 250 requests
rrrr[31144]: test client thread will send 250 requests
rrrr[31144]: test client thread will send 250 requests
rrrr[31111]: worker received 100 requests
rrrr[31112]: worker received 100 requests
rrrr[31110]: worker received 100 requests
rrrr[31113]: worker received 100 requests
rrrr[31109]: broker: frx 0502 ftx 0499 brx 0499 btx 0502 / 4 workers
rrrr[31112]: worker received 200 requests
rrrr[31111]: worker received 200 requests
rrrr[31113]: worker received 200 requests
rrrr[31110]: worker received 200 requests
rrrr[31144]: test client thread terminating
rrrr[31144]: test client thread terminating
rrrr[31144]: test client thread terminating
rrrr[31144]: test client thread terminating
rrrr[31144]: 4 threads, 1000 total requests, 3.893521 sec total time (256.836935 req/sec)
```

Testing Bliksem
-------------------

This project distinguishes between several types of tests. The **unit tests** check individual C units related to the RRRR router repository, such as the BitSet or RadixTree implementations. They make use of the `check` framework [http://check.sourceforge.net/]. The **functional tests** demonstrate that the Bliksem system meets the functional requirements of the MMRI project through which it was created. The **performance tests** demonstrate response time and throughput on journey planning operations connecting 50 points throughout the Netherlands. The performance tests and functional tests are types of **integration tests**, in that they test the process of issuing requests to a fully-assembled trip planner system rather than its individual components.

The `testerrrr` tool is a single-binary planner which allows planning simple queries without starting up separate broker or worker processes. It is simple to use this tool on a planner validation set such as the one used by MMRI.

Given a test, zip the test to a GTFS-file and compile the GTFS-file to a GTFSdb. Use the resulting GTFSdb as input for our timetable builder.

```bash
zip test.zip *.txt
gs_gtfsdb_compile test.zip test.gtfsdb
timetable.py test.gtfsdb
```

The tool can now be executed:

```bash
./testerrrr --from-idx=1 --to-idx=2 -a -D 2014-01-01T00:00:00 -T timetable.dat
./testerrrr --from-idx=1 --to-idx=2 -d -D 2014-01-01T00:00:00 -T timetable.dat
```

License
-------

Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. RRRR is released under the BSD 2-clause (simplified) license.

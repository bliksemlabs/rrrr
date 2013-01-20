RRRR Round-based Routing with RAPTOR
====================================

Introduction
------------

RRRR is a C-language implementation of the RAPTOR public transit routing algorithm. The goal of this project is to generate sets of Pareto-optimal itineraries over large geographic areas (e.g. BeNeLux or all of Europe), improving on the resource consumption and complexity of existing more flexible alternatives. The system should eventually support real-time vehicle/trip updates reflected in trip plans and be capable of running directly on a mobile device with no Internet connection.

In usual operation, RRRR exists as one or more FastCGI processes. A FastCGI-capable web server is configured to reverse-proxy CGI requests to these request handlers. Multiple RRRR processes running on the same machine map the same read-only data file into their private address space. This file contains a structured and indexed representation of transit timetables and other information from a GTFS feed. Additional handler processes should only increase physical memory consumption by the amount needed for search state (roughly 16 * num_stops * max_transfers bytes, on the order of a few megabytes). Eventually the real-time updater process will probably also use memory-mapped files for interprocess communication.

There will be little to no dynamic memory allocation and no multithreading -- each request handler is its own process and keeps a permanent scratch buffer that is reused from one request to the next. Stops are locations considered (streets will not be handled in the first phase of development). Eventually we will probably use protocol buffers over a 0MQ fan-out pattern to distribute real-time updates. This is basically standard GTFS-RT over protocol buffers instead of HTTP pull.

It looks like in may be possible to keep memory consumption for a Portland, Oregon-sized system under 10MiB.


Configuring Nginx for FastCGI
-----------------------------

Nginx can be configured to serve as a reverse proxy and pass off incoming requests to a pool of FastCGI processes like RRRR so as to distribute the load between them. This is also possible with Lighttpd and other web servers, but here I will give examples specifically for Nginx. 

We may start up the request handler processes either on the web server (in which case the server can communicate with RRRR via POSIX domain sockets) or on a separate machine (in which case they will communicate via TCP sockets). Domain sockets are identified using file-like paths and appear as special files in the filesystem. We suggest starting as many processes as your machine has cores.

You will first need to edit your site configuration file in `/etc/nginx/sites-enabled` to contain a section like the following:

```
location ~/rrrr {
    fastcgi_pass unix:/tmp/fastcgi.socket;
    fastcgi_param  QUERY_STRING     $query_string;
    fastcgi_param  REQUEST_METHOD   $request_method;
    fastcgi_param  CONTENT_TYPE     $content_type;
    fastcgi_param  CONTENT_LENGTH   $content_length;
}
```

Nginx will now yank the query string off the URL request and send it through to an RRRR worker process on the given socket. If you want to use a TCP socket, just replace `unix:/tmp/fastcgi.socket` with an ipaddr:port specification like `192.168.0.1:9090`. Note that Nginx will not automatically send any environment information (including QUERY_STRING) across to the handlers -- you have to configure it to do so by including variables in this configuration section.


Starting up RRRR
----------------

You can start fcgi handlers using cgi-fcgi (from the FastCGI developer's kit) or spawn-fcgi (available separately).

Domain socket: `cgi-fcgi -start -connect /tmp/fastcgi.socket ./rrrr $NUM_HANDLERS`

TCP socket: `cgi-fcgi -start -connect 192.168.10.10:9099 ./rrrr $NUM_HANDLERS`

The `restart.sh` script will kill off any running processes and start new ones (after a re-compile for example).

The domain socket file will be created automatically if it does not exist. However, if its permissions are not set correctly the web server will not be able to write to it.


Dependencies
------------

1. Graphserver must be installed for its SQLite GTFS tools
2. FastCGI Developer's Kit (Ubuntu package libfcgi-dev)
3. gcc

And nothing else besides a web server with FastCGI support!

Building RRRR
-------------

```bash
$ cd rrrr
$ make clean; make
````

Building transit data
---------------------

Download a GTFS feed for your favorite transit agency (stick with small ones for now).
Run `gs_gtfsdb_compile input.gtfs.zip output.gtfsdb` to load your GTFS feed into an sqlite database.
Then run `python timetable.py output.gtfsdb` to create the timetable file `/tmp/timetable.dat` based on that GTFS database.



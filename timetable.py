#!/usr/bin/python

import sys, struct
from struct import Struct
# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase

FILE = '/home/abyrd/trimet.gtfsdb'
FILE = '/home/abyrd/kv7.gtfsdb'
db = GTFSDatabase(FILE)

out = open("./timetable.dat", "wb")

#switch to unsigned
#pack arrival and departure into 4 bytes w/ offset?

struct_1i = Struct('i')
def writeint(x) :
    out.write(struct_1i.pack(x));

struct_1h = Struct('h')
def writeshort(x) :
    out.write(struct_1h.pack(x));

def tell() :
    pos = out.tell();
    if pos > 1024 * 1024 :
        text = '%0.2f MB' % (pos / 1024.0 / 1024.0)
    else :  
        text = '%0.2f kB' % (pos / 1024.0)
    print "  at position %d in output [%s]" % (pos, text)
    return pos

INTWIDTH = 4

# generator that takes a route (TripBundle) and returns all the stop times for each trip, 
# sorted by first departure time of each trip
def sorted_departure_times(db, bundle) :
    firststop = bundle.pattern.stop_ids[0]
    query = """
    select trip_id, departure_time from stop_times
    where stop_id = ?
    and trip_id in (%s)
    order by departure_time
    """ % (",".join( ["'%s'"%x for x in bundle.trip_ids] ))
    sorted_trips = [trip_id for (trip_id, departure_time) in db.execute(query, (firststop,))]
    # get all trip ids in this pattern ordered by first departure time
    for trip_id in sorted_trips :
        query = """
        select departure_time, stop_sequence
        from stop_times
        where trip_id = ?
        order by stop_sequence""" 
        for (departure_time, stop_sequence) in db.execute(query, (trip_id,)) :
            yield(departure_time)

struct_header = Struct('8s9i')
def write_header () :
    out.seek(0)
    htext = "TTABLEV1"
    packed = struct_header.pack(htext, nstops, nroutes, loc_stops, loc_routes, loc_route_stops, 
        loc_stop_times, loc_stop_routes, loc_transfers, loc_stop_ids)
    out.write(packed)
    
# seek past end of header, which will be written later
out.seek(struct_header.size)

print "building stop indexes and coordinate list"
struct_2f = Struct('2f')
# establish a mapping between sorted stop ids and integer indexes (allowing binary search later)
stopid_for_idx = []
idx_for_stopid = {}
idx = 0
query = """
        select stop_id, stop_name, stop_lat, stop_lon
        from stops
        order by stop_id
        """ 
for (sid, name, lat, lon) in db.execute(query) :
    idx_for_stopid[sid] = idx
    stopid_for_idx.append(sid)
    out.write(struct_2f.pack(lat, lon));
    idx += 1
nstops = idx
    
print "building trip bundles"
route_for_idx = db.compile_trip_bundles(reporter=sys.stdout)
nroutes = len(route_for_idx)

# named tuples?
# stops = [(-1, -1) for _ in range(nstops)]
# routes = [(-1, -1) for _ in range(nroutes)]

print "saving the stops in each route"
loc_route_stops = tell()
offset = 0
route_stops_offsets = []
for idx, route in enumerate(route_for_idx) :
    route_stops_offsets.append(offset)
    for sid in route.pattern.stop_ids :
        if sid in idx_for_stopid :
            writeint(idx_for_stopid[sid])
        else :
            print "route references unknown stop %s" % sid
            writeint(-1)
        offset += 1 
route_stops_offsets.append(offset) # sentinel
assert len(route_stops_offsets) == nroutes + 1

# print db.service_ids()

# what we are calling routes here are TripBundles in gtfsdb
print "saving the stop times for each trip of each route"
loc_stop_times = tell()
offset = 0
stop_times_offsets = []
for idx, route in enumerate(route_for_idx) :
    if idx > 0 and idx % 100 == 0 :
        print 'wrote %d routes' % idx
        tell()
    stop_times_offsets.append(offset)
    for departure_time in sorted_departure_times(db, route) :
        # could be stored as a short
        # writeshort(departure_time / 15)
        writeint(departure_time)
        offset += 1 
stop_times_offsets.append(offset) # sentinel
assert len(stop_times_offsets) == nroutes + 1

print "saving a list of routes serving each stop"
loc_stop_routes = tell()
stop_routes = {}
for idx, route in enumerate(route_for_idx) :
    for sid in route.pattern.stop_ids :
        if sid not in stop_routes :
            stop_routes[sid] = []
        stop_routes[sid].append(idx)

offset = 0
stop_routes_offsets = []
for idx in range(nstops) :
    stop_routes_offsets.append(offset)
    sid = stopid_for_idx[idx]
    if sid in stop_routes :
        for route_idx in stop_routes[sid] :
            writeint(route_idx)
            offset += 1 
stop_routes_offsets.append(offset) # sentinel
assert len(stop_routes_offsets) == nstops + 1

del stop_routes

print "saving transfers (footpaths)"
loc_transfers = tell()
offset = 0
transfers_offsets = []
query = """
select from_stop_id, to_stop_id, transfer_type, min_transfer_time
from transfers where from_stop_id = ?"""
struct_2i = Struct('if')
for from_idx, from_sid in enumerate(stopid_for_idx) :
    transfers_offsets.append(offset)
    for from_sid, to_sid, ttype, ttime in db.execute(query, (from_sid,)) :
        if ttime == None :
            continue # skip non-time/non-distance transfers for now
        to_idx = idx_for_stopid[to_sid]
        out.write(struct_2i.pack(to_idx, float(ttime))) # must convert time/dist
        offset += 1
transfers_offsets.append(offset) # sentinel
assert len(transfers_offsets) == nstops + 1
                                       
print "saving stop indexes"
loc_stops = tell()
struct_2i = Struct('ii')
for stop in zip (stop_routes_offsets, transfers_offsets) :
    out.write(struct_2i.pack(*stop));

print "saving route indexes"
loc_routes = tell()
for route in zip (route_stops_offsets, stop_times_offsets) :
    out.write(struct_2i.pack(*route));

print "writing out sorted stopids to string table"
# stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
width = 0;
for sid in stopid_for_idx :
    if len(sid) > width :
        width = len(sid)
width += 1
loc_stop_ids = tell()
writeint(width)
for sid in stopid_for_idx :
    out.write(sid)
    padding = '\0' * (width - len(sid))
    out.write(padding)

# maybe no need to store route IDs: report trip ids and look them up when reconstructing the response

    
print "reached end of timetable file"
loc_eof = tell()

print "rewinding and writing header... ",
write_header()
   
print "done."
out.close();


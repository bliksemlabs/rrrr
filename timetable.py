#!/usr/bin/python

import sys, struct
from struct import Struct
# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase

if len(sys.argv) != 2 :
    print 'usage: timetable.py inputfile.gtfsdb'
    exit(1)
    
gtfsdb_file = sys.argv[1]
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file %s cannot be opened' % gtfsdb_file
    exit(1)
out = open("./timetable.dat", "wb")
stops_out = open("./stops", "wb")

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

# function from a route (TripBundle) to a list of all trip_ids for that route,
# sorted by first departure time of each trip
def sorted_trip_ids(db, bundle) :
    firststop = bundle.pattern.stop_ids[0]
    query = """
    select trip_id, departure_time from stop_times
    where stop_id = ?
    and trip_id in (%s)
    order by departure_time
    """ % (",".join( ["'%s'"%x for x in bundle.trip_ids] ))
    # get all trip ids in this pattern ordered by first departure time
    sorted_trips = [trip_id for (trip_id, departure_time) in db.execute(query, (firststop,))]
    return sorted_trips
    
# generator that takes a list of trip_ids and returns all stop times in order for those trip_ids
def departure_times(trip_ids) :
    for trip_id in trip_ids :
        query = """
        select departure_time, stop_sequence
        from stop_times
        where trip_id = ?
        order by stop_sequence""" 
        for (departure_time, stop_sequence) in db.execute(query, (trip_id,)) :
            yield(departure_time)

struct_header = Struct('8s11i')
def write_header () :
    out.seek(0)
    htext = "TTABLEV1"
    packed = struct_header.pack(htext, nstops, nroutes, loc_stops, loc_routes, loc_route_stops, 
        loc_stop_times, loc_stop_routes, loc_transfers, loc_stop_ids, loc_route_ids, loc_trip_ids)
    out.write(packed)
    
# seek past end of header, which will be written later
out.seek(struct_header.size)

print "building stop indexes and coordinate list"
struct_2f = Struct('2f')
# establish a mapping between sorted stop ids and integer indexes (allowing binary search later)
stop_id_for_idx = []
idx_for_stopid = {}
idx = 0
query = """
        select stop_id, stop_name, stop_lat, stop_lon
        from stops
        order by stop_id
        """ 
for (sid, name, lat, lon) in db.execute(query) :
    idx_for_stopid[sid] = idx
    stop_id_for_idx.append(sid)
    out.write(struct_2f.pack(lat, lon));
    stops_out.write(name+'\n')
    idx += 1
nstops = idx
stops_out.close()
    
print "building trip bundles"
route_for_idx = db.compile_trip_bundles(reporter=sys.stdout)
nroutes = len(route_for_idx)

route_id_for_idx = []
for route in route_for_idx :
    rid = list(db.execute('select route_id from trips where trip_id = ?', (route.trip_ids[0],)))
    route_id_for_idx.append(rid[0][0])
    
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
stop_times_offsets = []
stoffset = 0
all_trip_ids = []
trip_ids_offsets = []
tioffset = 0
for idx, route in enumerate(route_for_idx) :
    if idx > 0 and idx % 100 == 0 :
        print 'wrote %d routes' % idx
        tell()
    # record the offset into the stop_times and trip_ids arrays for each trip block (route)
    stop_times_offsets.append(stoffset)
    trip_ids_offsets.append(tioffset)
    trip_ids = sorted_trip_ids(db, route)
    for departure_time in departure_times(trip_ids) :
        # could be stored as a short
        # writeshort(departure_time / 15)
        writeint(departure_time)
        stoffset += 1 
    all_trip_ids.extend(trip_ids)
    tioffset += len(trip_ids)
stop_times_offsets.append(stoffset) # sentinel
trip_ids_offsets.append(tioffset) # sentinel
assert len(stop_times_offsets) == nroutes + 1
assert len(trip_ids_offsets) == nroutes + 1


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
    sid = stop_id_for_idx[idx]
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
for from_idx, from_sid in enumerate(stop_id_for_idx) :
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
struct_3i = Struct('iii')
for route in zip (route_stops_offsets, stop_times_offsets, trip_ids_offsets) :
    out.write(struct_3i.pack(*route));

# write out a table of fixed width, zero-terminated strings and return the beginning file offset
def write_string_table (strings) :
    width = 0;
    for s in strings :
        if len(s) > width :
            width = len(s)
    width += 1
    loc = tell()
    writeint(width)
    for s in strings :
        out.write(s)
        padding = '\0' * (width - len(s))
        out.write(padding)
    return loc
    
print "writing out sorted stop ids to string table"
# stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
loc_stop_ids = write_string_table(stop_id_for_idx)

# maybe no need to store route IDs: report trip ids and look them up when reconstructing the response
print "writing route ids to string table"
loc_route_ids = write_string_table(route_id_for_idx)

print "writing trip ids to string table"
loc_trip_ids = write_string_table(all_trip_ids)
    
print "reached end of timetable file"
loc_eof = tell()

print "rewinding and writing header... ",
write_header()
   
print "done."
out.close();


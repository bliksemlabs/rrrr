#!/usr/bin/python

import sys, struct
from struct import Struct
# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase
from datetime import timedelta, date

if len(sys.argv) != 3 :
    print 'usage: timetable.py inputfile.gtfsdb YYYY-MM-DD'
    exit(1)
    
gtfsdb_file = sys.argv[1]
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file %s cannot be opened' % gtfsdb_file
    exit(1)
out = open("./timetable.dat", "wb")
stops_out = open("./stops", "wb") # ID <-> StopName map for the geocoder



#############CALENDAR


# second param: 32-day calendar start date
start_date = date( *map(int, sys.argv[2].split('-')) )
print 'calendar start date is %s' % start_date

# db.date_range() is somewhat slow.
# db.service_periods(sample_date) is slow because it checks its parameters with date_range().
sids = db.service_ids()
print '%d distinct service IDs' % len(sids)
print 'feed covers %s -- %s' % db.date_range()

# find active period
#dfrom, dto = db.date_range()
#d = dfrom
#while (d <= dto) :
#    active_sids = db.service_periods(d)
#    print d, len(active_sids)
#    d += timedelta(days = 1)

bitmask_for_sid = {}
for sid in sids :
    bitmask_for_sid[sid] = 0
for day_offset in range(32) :
    date = start_date + timedelta(days = day_offset)
    active_sids = db.service_periods(date)
    day_mask = 1 << day_offset
    print 'date {!s} has {:d} active service ids. applying mask (base2) {:032b}'.format(date, len(active_sids), day_mask)
    for sid in active_sids :
        bitmask_for_sid[sid] |= day_mask

for sid in sids :
    print '{:<5s} {:032b}'.format(sid, bitmask_for_sid[sid])
        
service_id_for_trip_id = {}        
query = """ select trip_id, service_id from trips """
for (tid, sid) in db.execute(query) :
    service_id_for_trip_id [tid] = sid

############################


#switch to unsigned
#pack arrival and departure into 4 bytes w/ offset?

struct_1i = Struct('I') # a single UNSIGNED int
def writeint(x) :
    out.write(struct_1i.pack(x));

struct_1h = Struct('h') # a single short 
def writeshort(x) : 
    out.write(struct_1h.pack(x));

struct_2f = Struct('2f') # 2 floats
def write2floats(x, y) :
    out.write(struct_2f.pack(x, y));

def align(width) :
    """ Align output file to a [width]-byte boundary. """
    pos = out.tell()
    n_padding_bytes = (width - (pos % width)) % width
    out.write('%' * n_padding_bytes)
    
def tell() :
    """ Display the current output file position in a human-readable format, then return that position in bytes. """
    pos = out.tell()
    if pos > 1024 * 1024 :
        text = '%0.2f MB' % (pos / 1024.0 / 1024.0)
    else :  
        text = '%0.2f kB' % (pos / 1024.0)
    print "  at position %d in output [%s]" % (pos, text)
    return pos

def write_text_comment(string) :
    """ Write a text block to the file, just to help indentify segment boundaries, and align. """
    string = '|| {:s} ||'.format(string)
    out.write(string) 
    align(4)

def write_string_table(strings) :
    """ Write a table of fixed-width, null-terminated strings to the output file.
    The argument is a list of Python strings.
    The output string table begins with an integer indicating the width of each entry in bytes (including the null terminator).
    This is followed by N*W bytes of string data.
    This data structure can provide a mapping from integer IDs to strings and vice versa:
    If the strings are sorted, a binary search can be used for string --> ID lookups in logarithmic time.
    Note: Later we could use fixed width non-null-terminated string: printf("%.*s", length, string); or fwrite();
    """
    # sort a copy of the string list
    # strings = list(strings) 
    # strings.sort()
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

def sorted_trip_ids(db, bundle) :
    """ function from a route (TripBundle) to a list of all trip_ids for that route,
    sorted by first departure time of each trip """
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
    
def departure_times(trip_ids) :
    """ generator that takes a list of trip_ids 
    and returns all stop times in order for those trip_ids """
    for trip_id in trip_ids :
        query = """
        select departure_time, stop_sequence
        from stop_times
        where trip_id = ?
        order by stop_sequence""" 
        for (departure_time, stop_sequence) in db.execute(query, (trip_id,)) :
            yield(departure_time)

struct_header = Struct('8s12i')
def write_header () :
    out.seek(0)
    htext = "TTABLEV1"
    packed = struct_header.pack(htext, nstops, nroutes, loc_stops, loc_routes, loc_route_stops, 
        loc_stop_times, loc_stop_routes, loc_transfers, 
        loc_stop_ids, loc_route_ids, loc_trip_ids, loc_trip_active)
    out.write(packed)
    
# seek past end of header, which will be written later
out.seek(struct_header.size)

print "building stop indexes and coordinate list"
# establish a mapping between sorted stop ids and integer indexes (allowing binary search later)
stop_id_for_idx = []
idx_for_stop_id = {}
idx = 0
query = """
        select stop_id, stop_name, stop_lat, stop_lon
        from stops
        order by stop_id
        """
# loop over all stop IDs in alphabetical order
for (sid, name, lat, lon) in db.execute(query) :
    idx_for_stop_id[sid] = idx
    stop_id_for_idx.append(sid)
    write2floats(lat, lon)
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
write_text_comment("STOPS BY ROUTE")
loc_route_stops = tell()
offset = 0
route_stops_offsets = []
for idx, route in enumerate(route_for_idx) :
    route_stops_offsets.append(offset)
    for sid in route.pattern.stop_ids :
        if sid in idx_for_stop_id :
            writeint(idx_for_stop_id[sid])
        else :
            print "route references unknown stop %s" % sid
            writeint(-1)
        offset += 1 
route_stops_offsets.append(offset) # sentinel
assert len(route_stops_offsets) == nroutes + 1

# print db.service_ids()

# what we are calling routes here are TripBundles in gtfsdb
print "saving the stop times for each trip of each route"
write_text_comment("STOP TIMES")
loc_stop_times = tell()
stop_times_offsets = []
stoffset = 0
all_trip_ids = []
trip_ids_offsets = [] # also serves as offsets into per-trip "service active" bitfields
tioffset = 0
for idx, route in enumerate(route_for_idx) :
    if idx > 0 and idx % 1000 == 0 :
        print 'wrote %d routes' % idx
        tell()
    # record the offset into the stop_times and trip_ids arrays for each trip block (route)
    stop_times_offsets.append(stoffset)
    trip_ids_offsets.append(tioffset)
    trip_ids = sorted_trip_ids(db, route)
    
    ### A route/tripbundle may have multiple serviceIds, but often covers only some days or is zero
    ### filter out inactive routes on the search day
    # mask = 0
    # for trip_id in trip_ids :
    #     mask |= bitmask_for_sid[ service_id_for_trip_id [trip_id] ]
    # print 'mask for all trips is {:032b}'.format(mask)
    ###
    
    # print idx, route, len(trip_ids)
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
write_text_comment("ROUTES BY STOP")
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
write_text_comment("TRANSFERS BY STOP")
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
        to_idx = idx_for_stop_id[to_sid]
        out.write(struct_2i.pack(to_idx, float(ttime))) # must convert time/dist
        offset += 1
transfers_offsets.append(offset) # sentinel
assert len(transfers_offsets) == nstops + 1
                                       
print "saving stop indexes"
write_text_comment("STOP STRUCTS")
loc_stops = tell()
struct_2i = Struct('ii')
for stop in zip (stop_routes_offsets, transfers_offsets) :
    out.write(struct_2i.pack(*stop));

print "saving route indexes"
write_text_comment("ROUTE STRUCTS")
loc_routes = tell()
struct_3i = Struct('iii')
for route in zip (route_stops_offsets, stop_times_offsets, trip_ids_offsets) :
    out.write(struct_3i.pack(*route));

print "writing out sorted stop ids to string table"
# stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
write_text_comment("STOP IDS (SORTED)")
loc_stop_ids = write_string_table(stop_id_for_idx)

# maybe no need to store route IDs: report trip ids and look them up when reconstructing the response
print "writing route ids to string table"
write_text_comment("ROUTE IDS")
loc_route_ids = write_string_table(route_id_for_idx)

print "writing trip ids to string table" 
# note that trip_ids are ordered by departure time within trip bundles (routes), which are themselves in arbitrary order. 
write_text_comment("TRIP IDS")
loc_trip_ids = write_string_table(all_trip_ids)

print "writing bitfields indicating which days each trip is active" 
# note that bitfields are ordered identically to the trip_ids table, and offsets into that table can be reused
write_text_comment("SERVICE ACTIVE BITFIELDS")
loc_trip_active = tell()
n_zeros = 0
for trip_id in all_trip_ids :
    service_id = service_id_for_trip_id [trip_id]
    try:
        bitmask = bitmask_for_sid [service_id]
    except:
        bitmask = 0
        print 'Trip_id %s is missing %s' % (trip_id,service_id)
    if bitmask == 0 :
        n_zeros += 1
    # print '{:032b} {:s} ({:s})'.format(bitmask, trip_id, service_id)
    writeint(bitmask)
print '(%d / %d bitmasks were zero)' % ( n_zeros, len(all_trip_ids) )

print "reached end of timetable file"
write_text_comment("END TTABLEV1")
loc_eof = tell()

print "rewinding and writing header... ",
write_header()
   
print "done."
out.close();


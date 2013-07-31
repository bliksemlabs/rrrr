#!/usr/bin/python

import sys, struct, time
from struct import Struct
# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase
import datetime
from datetime import timedelta, date

if len(sys.argv) < 2 :
    USAGE = """usage: timetable.py inputfile.gtfsdb [calendar start date] 
    If a start date is provided in YYYY-MM-DD format, a calendar will be built for the 32 days following the given date. 
    Otherwise the service calendar will be analyzed and the month with the maximum number of running services will be used."""
    print USAGE
    exit(1)

# first command line parameter: the gtfsdb to convert    
gtfsdb_file = sys.argv[1]
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file %s cannot be opened' % gtfsdb_file
    exit(1)
    
out = open("./timetable.dat", "wb")
stops_out = open("./stops", "wb") # ID <-> StopName map for the geocoder

feed_start_date, feed_end_date = db.date_range()
print 'feed covers %s -- %s' % (feed_start_date, feed_end_date)

def find_max_service () :
    n_services_on_day = []
    # apply calendars (day-of-week)
    day_masks = [ [ x != 0 for x in c[1:8] ] for c in db.execute('select * from calendar') ]
    feed_date = feed_start_date
    while feed_date <= feed_end_date :
        feed_date += timedelta(days = 1)
        count = 0
        for day_mask in day_masks :
            if day_mask[feed_date.weekday()] :
                count += 1
        n_services_on_day.append(count)
    # apply single-day exceptions
    for service_id, text_date, exception_type in db.execute('select * from calendar_dates') :
        service_date = date( *map(int, (text_date[:4], text_date[4:6], text_date[6:8])) )
        date_offset = service_date - feed_start_date
        day_offset = date_offset.days
        # print service_date, day_offset
        if exception_type == 1 :
            n_services_on_day[day_offset] += 1
        else :
            n_services_on_day[day_offset] -= 1
    n_month = [ sum(n_services_on_day[i:i+32]) for i in range(len(n_services_on_day) - 32) ]
    max_date, max_n_services = max(enumerate(n_month), key = lambda x : x[1])
    max_date = feed_start_date + timedelta(days = max_date)
    print "32-day period with the maximum number of services begins %s (%d services)" % (max_date, max_n_services)  
    return max_date 

# second command line parameter: start date for 32-day calendar
try :
    start_date = date( *map(int, sys.argv[2].split('-')) )
except :
    print 'Scanning service calendar to find the month with maximum service.'
    print 'NOTE that this is not necessarily accurate and you can end up with sparse service in the chosen period.'
    start_date = find_max_service()
print 'calendar start date is %s' % start_date
calendar_start_time = time.mktime(datetime.datetime.combine(start_date, datetime.time.min).timetuple())
print 'epoch time at which calendar starts: %d' % calendar_start_time

sids = db.service_ids()
print '%d distinct service IDs' % len(sids)
                    
bitmask_for_sid = {}
for sid in sids :
    bitmask_for_sid[sid] = 0
for day_offset in range(32) :
    date = start_date + timedelta(days = day_offset)
    # db.date_range() is somewhat slow.
    # db.service_periods(sample_date) is slow because it checks its parameters with date_range().
    # this is very inefficient, but run time is reasonable for now and it uses existing code.
    active_sids = db.service_periods(date)
    day_mask = 1 << day_offset
    print 'date {!s} has {:5d} active service ids. applying mask {:032b}'.format(date, len(active_sids), day_mask)
    for sid in active_sids :
        bitmask_for_sid[sid] |= day_mask

#for sid in sids :
#    print '{:<5s} {:032b}'.format(sid, bitmask_for_sid[sid])
        
service_id_for_trip_id = {}        
query = """ select trip_id, service_id from trips """
for (tid, sid) in db.execute(query) :
    service_id_for_trip_id [tid] = sid

############################


#pack arrival and departure into 4 bytes w/ offset?

# C structs, must match those defined in .h files.

struct_1I = Struct('I')
def writeint(x) :
    out.write(struct_1I.pack(x));

struct_2H = Struct('HH') 
def write_2ushort(x, y) : 
    out.write(struct_2H.pack(x, y));
        
struct_2f = Struct('2f') # 2 floats
def write2floats(x, y) :
    out.write(struct_2f.pack(x, y));

def align(width=4) :
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
    align()

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
    
def fetch_stop_times(trip_ids) :
    """ generator that takes a list of trip_ids 
    and returns all stop times in order for those trip_ids """
    for trip_id in trip_ids :
        last_time = 0
        query = """
        select arrival_time, departure_time, stop_sequence
        from stop_times
        where trip_id = ?
        order by stop_sequence""" 
        for (arrival_time, departure_time, stop_sequence) in db.execute(query, (trip_id,)) :
            if departure_time < last_time :
                print "non-increasing departure times on trip %s, forcing positive" % trip_id
                departure_time = last_time
            last_time = departure_time
            yield(arrival_time, departure_time)

# make this into a method on a Header class 
struct_header = Struct('8sQ14I')
def write_header () :
    """ Write out a file header containing offsets to the beginning of each subsection. 
    Must match struct transit_data_header in transitdata.c """
    out.seek(0)
    htext = "TTABLEV1"
    packed = struct_header.pack(htext, calendar_start_time, nstops, nroutes, loc_stops, loc_stop_coords,
        loc_routes, loc_route_stops, loc_stop_times, loc_stop_routes, loc_transfers, 
        loc_stop_ids, loc_route_ids, loc_trip_ids, loc_trip_active, loc_route_active)
    out.write(packed)

### Begin writing out file ###
    
# Seek past the end of the header, which will be written last when all offsets are known.
out.seek(struct_header.size)

print "building stop indexes and coordinate list"
# establish a mapping between sorted stop ids and integer indexes (allowing binary search later)
stop_id_for_idx = []
stop_name_for_idx = []
idx_for_stop_id = {}
idx = 0
query = """
        select stop_id, stop_name, stop_lat, stop_lon
        from stops
        order by stop_id
        """
# Write timetable segment 0 : stop coordinates
# (loop over all stop IDs in alphabetical order)
loc_stop_coords = out.tell() 
for (sid, name, lat, lon) in db.execute(query) :
    idx_for_stop_id[sid] = idx
    stop_id_for_idx.append(sid)
    stop_name_for_idx.append(name)
    write2floats(lat, lon)
    stops_out.write(name+'\n')
    idx += 1
nstops = idx
stops_out.close()
    
print "building trip bundles"
all_routes = db.compile_trip_bundles(reporter=sys.stdout) # slow call
# A route ("TripBundle") may have many service_ids, and often runs only some days or none at all.
# Throw out routes and trips that do not occur during this calendar period,
# and record active-days-bitmasks for each route, allowing us to filter out inactive routes on a specific search day.
route_mask_for_idx = [] # one active-days-bitmask for each route (bundle of trips)
route_for_idx = []
route_n_stops = [] # number of stops in each route
route_n_trips = [] # number of trips in each route
n_trips_total = 0
n_trips_removed = 0
n_routes_removed = 0
for route in all_routes :
    route_mask = 0
    running_trip_ids = []
    n_trips_total += len(route.trip_ids)
    for trip_id in route.trip_ids :
        try :
            service_id = service_id_for_trip_id [trip_id]
            trip_mask = bitmask_for_sid[service_id]
            route_mask |= trip_mask
            if trip_mask != 0 :
                running_trip_ids.append(trip_id)
            else :
                n_trips_removed += 1
        except KeyError:
            continue # might this accidentally get the lists out of sync?
    # print 'mask for all trips is {:032b}'.format(route_mask)
    if route_mask != 0 :
        route.trip_ids = running_trip_ids
        route_for_idx.append(route)
        route_mask_for_idx.append(route_mask)
        route_n_stops.append(len(route.pattern.stop_ids))
        route_n_trips.append(len(running_trip_ids))
    else :
        n_routes_removed += 1
nroutes = len(route_for_idx) # this is the final culled list of routes
print '%d / %d routes had running services, %d / %d trips removed' % (nroutes, len(all_routes), n_trips_removed, n_trips_total)
del all_routes, n_trips_total, n_trips_removed, n_routes_removed
route_n_stops.append(0) # sentinel
route_n_trips.append(0) # sentinel

# We have two definitions of "route".
# GTFS routes are totally arbitrary groups of trips (but fortunately NL GTFS routes correspond with 
# rider notions of a route).
# RAPTOR routes are what Graphserver calls trip bundles and OTP calls stop patterns and Transmodel 
# calls JOURNEYPATTERNs: an ordered sequence of stops.
# We want one descriptive string per route, in the RAPTOR sense, which should amount to one string 
# per GTFS route per direction.
route_id_for_idx = []
modes = { 0: 'tram', 
          1: 'subway',
          2: 'train',
          3: 'bus',
          4: 'ferry',
          5: 'cable car',
          6: 'gondola',
          7: 'funicular' }
for route in route_for_idx :
    exemplar_trip = route.trip_ids[0]
    SQL = """ select trips.route_id, trips.trip_headsign, 
                     routes.agency_id, routes.route_short_name, routes.route_long_name, routes.route_type
              from trips, routes
              where trips.trip_id = ?
              and trips.route_id = routes.route_id """
    #  executemany
    rid, headsign, agency, short_name, long_name, mode = list(db.execute(SQL, (exemplar_trip,)))[0]    
    if short_name is None : 
        # mostly trains with the service type (Sprinter, IC) in the long name field
        desc = long_name
    else : 
        desc = '%s %s' % (modes[mode], short_name)
        if long_name is not None :
            desc += ' (%s)' % long_name
    if (headsign is not None) :
        desc = ';'.join([desc, headsign])
    # print desc
    route_id_for_idx.append(desc)

    
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

def time_string_from_seconds(sec) :
    t = time.localtime(sec)
    #if t.tm_mday > 0 :
    return time.strftime('day %d %H:%M:%S', t)

# what we are calling routes here are TripBundles in gtfsdb
print "saving the stop times for each trip of each route"
# In the full NL dataset only about half of route (RAPTOR sense) have any dwells at all.
# Rather than try to do some intense bit-twiddling time packing on all routes, we'll just 
# check ahead of time whether each route has dwells or not, store separate arrivals and 
# departures table offsets, which point to the same location if the route has no dwells. This
# should achieve about 25% space savings with very simple implementation.
# The problem is that this gets in the way of using the address of the next route block as a termination condition for loops.
# The pointer arithmetic would need to be replaced with array indexing (this has now been done, so the change can be made).
write_text_comment("STOP TIMES")
loc_stop_times = tell()
stop_times_offsets = []
stoffset = 0
all_trip_ids = []
trip_ids_offsets = [] # also serves as offsets into per-trip "service active" bitfields
tioffset = 0
two_days = 60 * 60 * 24 * 2
rhf = open('route_hours.txt', 'w')
for idx, route in enumerate(route_for_idx) :
    if idx > 0 and idx % 1000 == 0 :
        print 'wrote %d routes' % idx
        tell()
    # record the offset into the stop_times and trip_ids arrays for each trip block (route)
    stop_times_offsets.append(stoffset)
    trip_ids_offsets.append(tioffset)
    trip_ids = sorted_trip_ids(db, route)
    # print idx, route, len(trip_ids)
    hours_active = [False] * 48
    min_hour = 47
    max_hour = 0
    for arrival_time, departure_time in fetch_stop_times(trip_ids) :
        # 2**16 / 60 / 60 is only 18 hours
        # by right-shifting all times one bit we get 36 hours (1.5 days) at 2 second resolution
        if departure_time < arrival_time :
            print "negative dwell time"
            # do not write UNREACHABLE, this may cause problems
            write_2ushort(two_days >> 2, two_days >> 2)
        elif departure_time > two_days :
            print 'time greater than two days:', departure_time, time_string_from_seconds(departure_time)
            write_2ushort(two_days >> 2, two_days >> 2)
        else :
            write_2ushort(arrival_time >> 2, departure_time >> 2)
            hour = arrival_time / 60 / 60
            hours_active[hour] = True
            if hour < min_hour :
                min_hour = hour
            if hour > max_hour :
                max_hour = hour
        stoffset += 1 
    hours = ''.join([('X' if h else '_') for h in hours_active])
    hours_string = ('%s|%s min=%d max=%d \n' % (hours[:24], hours[24:], min_hour, max_hour))
    rhf.write(hours_string)
    # print hours_string
    all_trip_ids.extend(trip_ids)
    tioffset += len(trip_ids)
stop_times_offsets.append(stoffset) # sentinel
trip_ids_offsets.append(tioffset) # sentinel
assert len(stop_times_offsets) == nroutes + 1
assert len(trip_ids_offsets) == nroutes + 1
rhf.close()

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
struct_If = Struct('If')
for from_idx, from_sid in enumerate(stop_id_for_idx) :
    transfers_offsets.append(offset)
    for from_sid, to_sid, ttype, ttime in db.execute(query, (from_sid,)) :
        if ttime == None :
            if ttype <= 2:
                ttime = 0
            else:
                continue
        to_idx = idx_for_stop_id[to_sid]
        out.write(struct_If.pack(to_idx, float(ttime))) # must convert time/dist
        offset += 1
transfers_offsets.append(offset) # sentinel
assert len(transfers_offsets) == nstops + 1
                                       
print "saving stop indexes"
write_text_comment("STOP STRUCTS")
loc_stops = tell()
struct_2I = Struct('II')
for stop in zip (stop_routes_offsets, transfers_offsets) :
    out.write(struct_2I.pack(*stop));

print "saving route indexes"
write_text_comment("ROUTE STRUCTS")
loc_routes = tell()
route_t = Struct('IIIII')
route_t_fields = [route_stops_offsets, stop_times_offsets, trip_ids_offsets, route_n_stops, route_n_trips]
# check that all list lengths match the total number of routes. 
for l in route_t_fields :
    # the extra last route is a sentinel so we can derive list lengths for the last true route.
    assert len(l) == nroutes + 1
for route in zip (*route_t_fields) :
    # print route
    out.write(route_t.pack(*route));

print "writing out sorted stop ids to string table"
# stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
write_text_comment("STOP IDS (SORTED)")
# loc_stop_ids = write_string_table(stop_id_for_idx) <-- write IDs instead of names
loc_stop_ids = write_string_table(stop_name_for_idx)

# maybe no need to store route IDs: report trip ids and look them up when reconstructing the response
print "writing route ids to string table"
write_text_comment("ROUTE IDS")
loc_route_ids = write_string_table(route_id_for_idx)


print "writing trip ids to string table" 
# note that trip_ids are ordered by departure time within trip bundles (routes), which are themselves in arbitrary order. 
write_text_comment("TRIP IDS")
#loc_trip_ids = write_string_table(all_trip_ids)
loc_trip_ids = 0

print "writing bitfields indicating which days each trip is active" 
# note that bitfields are ordered identically to the trip_ids table, and offsets into that table can be reused
write_text_comment("TRIP ACTIVE BITFIELDS")
loc_trip_active = tell()
n_zeros = 0
for trip_id in all_trip_ids :
    service_id = service_id_for_trip_id [trip_id]
    try :
        bitmask = bitmask_for_sid [service_id]
    except :
        print 'no calendar information for service_id', service_id
        bitmask = 0
    if bitmask == 0 :
        n_zeros += 1
    # print '{:032b} {:s} ({:s})'.format(bitmask, trip_id, service_id)
    writeint(bitmask)
print '(%d / %d bitmasks were zero)' % ( n_zeros, len(all_trip_ids) )


print "writing bitfields indicating which days each route is active" 
write_text_comment("ROUTE ACTIVE BITFIELDS")
loc_route_active = tell()
n_zeros = 0
for bitfield in route_mask_for_idx :
    if bitfield == 0 :
        n_zeros += 1
    writeint(bitfield)
print '(%d / %d bitmasks were zero)' % ( n_zeros, len(all_trip_ids) )


print "reached end of timetable file"
write_text_comment("END TTABLEV1")
loc_eof = tell()

print "rewinding and writing header... ",
write_header()
   
print "done."
out.close();




def analyze_dwells () :
    """ Use this method to discover than in the full NL dataset only about half of route (RAPTOR sense) have any dwells at all. """
    dwells = {} # histogram of dwell times
    routes_with_dwells = 0
    trips_with_dwells = 0
    isolated_dwells = 0
    ntrips = 0
    ndwells = 0
    for route in route_for_idx :
        route_has_dwells = False
        SQL = "select arrival_time, departure_time from stop_times where trip_id = ? order by stop_sequence"    
        for trip_id in route.trip_ids :
            ntrips += 1
            trip_has_dwells = False
            prev_no_dwell = False # at beginning of trip there is no reference for a relative stoptime
            for arrival_time, departure_time in db.execute(SQL, (trip_id,)) :
                dwell = departure_time - arrival_time
                if dwell > 0 :
                    route_has_dwells = True
                    trip_has_dwells = True
                    if prev_has_dwell :
                        isolated_dwells += 1
                    prev_has_dwell = True # for next iteration
                    ndwells += 1
                else :
                    prev_has_dwell = False # for next iteration
                try :
                    dwells[dwell] += 1
                except KeyError :
                    dwells[dwell] = 1
            if trip_has_dwells :
                trips_with_dwells += 1
        if route_has_dwells :
            routes_with_dwells += 1
    print "DWELL SUMMARY"
    print "routes with dwells %d / %d (%f%%)" % (routes_with_dwells, len(route_for_idx), routes_with_dwells * 100.0 / len(route_for_idx))
    print "trips with dwells %d / %d (%f%%)" % (trips_with_dwells, ntrips, trips_with_dwells * 100.0 / ntrips)
    print "isolated dwells %d / %d (%f%%)" % (isolated_dwells, ndwells, isolated_dwells * 100.0 / ndwells)
    print "HISTOGRAM"
    print "dwell, frequency"
    for dwell, n in dwells.items() :
        print '%d,%d' % (dwell, n)
    sys.exit(0)
    

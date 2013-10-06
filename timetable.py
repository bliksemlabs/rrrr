#!/usr/bin/env python2
import sys, struct, time
from struct import Struct
import datetime
from datetime import timedelta, date
from gtfsdb import GTFSDatabase
import os
import sqlite3
import operator

MAX_DISTANCE = 801

if len(sys.argv) < 2 :
    USAGE = """usage: timetable.py inputfile.gtfsdb [calendar start date] 
    If a start date is provided in YYYY-MM-DD format, a calendar will be built for the 32 days following the given date. 
    Otherwise the service calendar will be analyzed and the month with the maximum number of running services will be used."""
    print USAGE
    exit(1)

db = GTFSDatabase(sys.argv[1])    

out = open("./timetable.dat", "wb")

feed_start_date, feed_end_date = db.date_range()
print 'feed covers %s -- %s' % (feed_start_date, feed_end_date)

# second command line parameter: start date for 32-day calendar
try :
    start_date = date( *map(int, sys.argv[2].split('-')) )
except :
    print 'Scanning service calendar to find the month with maximum service.'
    print 'NOTE that this is not necessarily accurate and you can end up with sparse service in the chosen period.'
    start_date = db.find_max_service()
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
for tid, sid in db.tripids_in_serviceperiods() :
    service_id_for_trip_id [tid] = sid

############################

#pack arrival and departure into 4 bytes w/ offset?

# C structs, must match those defined in .h files.

struct_1I = Struct('I') # a single UNSIGNED int
def writeint(x) :
    out.write(struct_1I.pack(x));

struct_1B = Struct('B') # a single UNSIGNED byte
def writebyte(x) :
    out.write(struct_1B.pack(x));

struct_2H = Struct('HH') # a two UNSIGNED shorts
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

# make this into a method on a Header class 
# On 64-bit architectures using gcc long int is at least an int64_t.
# We were using L in platform dependent mode, which just happened to work. TODO switch to platform independent mode?
struct_header = Struct('8sQ29I') 
def write_header () :
    """ Write out a file header containing offsets to the beginning of each subsection. 
    Must match struct transit_data_header in transitdata.c """
    out.seek(0)
    htext = "TTABLEV1"
    packed = struct_header.pack(htext,
        calendar_start_time,
        nstops,
        nroutes,
        len(all_trip_ids),
        loc_stops,
        loc_stop_attributes,
        loc_stop_coords,
        loc_routes,
        loc_route_stops,
        loc_route_stop_attributes, 
        loc_timedemandgroups,
        loc_trips,
        loc_trip_attributes,
        loc_stop_routes,
        loc_transfer_target_stops,
        loc_transfer_dist_meters,
        loc_trip_active,
        loc_route_active,
        loc_platformcodes,
        loc_stop_names,
        loc_stop_nameidx,
        loc_agency_ids,
        loc_agency_names,
        loc_agency_urls,
        loc_headsign,
        loc_route_shortnames,
        loc_productcategories,
        loc_route_ids,
        loc_stop_ids,
        loc_trip_ids,
    )
    out.write(packed)

### Begin writing out file ###
    
# Seek past the end of the header, which will be written last when all offsets are known.
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
try:
    os.remove('stops.db')
except:
    pass
conn = sqlite3.connect("stops.db")
conn.text_factory = str
cur = conn.cursor()
cur.execute("""
CREATE VIRTUAL TABLE stops_fts USING FTS4 (
    stopindex integer primary key,
    stopname TEXT
);""")
stopnames = set([])
# Write timetable segment 0 : stop coordinates
# (loop over all stop IDs in alphabetical order)
loc_stop_coords = out.tell()
nameloc_for_name = {}
nameloc_for_idx = []
namesize = 0
platformcode_for_idx = []
for sid, name, lat, lon, platform_code in db.stops() :
    platform_code = platform_code or ''
    idx_for_stop_id[sid] = idx
    stop_id_for_idx.append(sid)
    write2floats(lat, lon)
    platformcode_for_idx.append(platform_code)
    if name in nameloc_for_name:
        nameloc = nameloc_for_name[name]
    else:
        nameloc = namesize
        nameloc_for_name[name] = nameloc
        namesize += 1 + len(name)
    nameloc_for_idx.append(nameloc)
    if name not in stopnames:
        stopnames.add(name)
        cur.execute('INSERT INTO stops_fts VALUES (?,?)',(idx,name))
    idx += 1
nstops = idx
assert len(nameloc_for_idx) == idx
conn.commit()
conn.close()
del stopnames
    
print "building trip bundles"
all_routes = db.compile_trip_bundles(reporter=sys.stdout) # slow call
# A route ("TripBundle") may have many service_ids, and often runs only some days or none at all.
# Throw out routes and trips that do not occur during this calendar period,
# and record active-days-bitmasks for each route, allowing us to filter out inactive routes on a specific search day.
route_mask_for_idx = [] # one active-days-bitmask for each route (bundle of trips)
route_for_idx = []
route_n_stops = [] # number of stops in each route
route_n_trips = [] # number of trips in each route
route_min_time = []
route_max_time = []
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
        min_time, max_time = route.find_time_range()
        route_min_time.append(min_time >> 2)
        route_max_time.append(max_time >> 2)
    else :
        n_routes_removed += 1
nroutes = len(route_for_idx) # this is the final culled list of routes
print '%d / %d routes had running services, %d / %d trips removed' % (nroutes, len(all_routes), n_trips_removed, n_trips_total)
del all_routes, n_trips_total, n_trips_removed, n_routes_removed
route_n_stops.append(0) # sentinel
route_n_trips.append(0) # sentinel
route_min_time.append(0) # sentinel
route_max_time.append(0) # sentinel

# We have two definitions of "route".
# GTFS routes are totally arbitrary groups of trips (but fortunately NL GTFS routes correspond with 
# rider notions of a route).
# RAPTOR routes are what Graphserver calls trip bundles and OTP calls stop patterns and Transmodel 
# calls JOURNEYPATTERNs: an ordered sequence of stops.
# We want one descriptive string per route, in the RAPTOR sense, which should amount to one string 
# per GTFS route per direction.
route_ids_for_idx = []
route_attributes = []
agency_offsets = []
shortname_offsets = []
headsign_offsets = []
productcategory_offsets = []
idx_for_agency = {}
idx_for_headsign = {}
idx_for_shortname = {}
idx_for_productcategory = {}
for route in route_for_idx :
    exemplar_trip = route.trip_ids[0]
    #  executemany
    productcategory = ''
    rid, headsign, agency, short_name, long_name, mode = db.tripinfo(exemplar_trip)
    if (headsign is None) :
        headsign = ''
    route_ids_for_idx.append(rid)
    route_attributes.append(1 << mode)

    agency = agency or ''
    headsign = headsign or ''
    short_name = short_name or ''
    productcategory = productcategory or ''
    if agency in idx_for_agency:
        agency_offset = idx_for_agency[agency]
    else:
        agency_offset = len(idx_for_agency)
        idx_for_agency[agency] = agency_offset
    agency_offsets.append(agency_offset)

    if headsign in idx_for_headsign:
        headsign_offset = idx_for_headsign[headsign]
    else:
        headsign_offset = len(idx_for_headsign)
        idx_for_headsign[headsign] = headsign_offset
    headsign_offsets.append(headsign_offset)

    if short_name in idx_for_shortname:
        shortname_offset = idx_for_shortname[short_name]
    else:
        shortname_offset = len(idx_for_shortname)
        idx_for_shortname[short_name] = shortname_offset
    shortname_offsets.append(shortname_offset)

    if productcategory in idx_for_productcategory:
        productcategory_offset = idx_for_productcategory[productcategory]
    else:
        productcategory_offset = len(idx_for_productcategory)
        idx_for_productcategory[productcategory] = productcategory_offset
    productcategory_offsets.append(productcategory_offset)
route_attributes.append(0) # sentinel
agency_offsets.append(0) # sentinel  
headsign_offsets.append(0) # sentinel
shortname_offsets.append(0) # sentinel
productcategory_offsets.append(0) # sentinel
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

print "saving attributes of stops in each route"
write_text_comment("STOPS ATTRIBUTES BY ROUTE")
loc_route_stop_attributes = tell()
offset = 0
route_stops_attributes_offsets = []
for idx, route in enumerate(route_for_idx) :
    route_stops_attributes_offsets.append(offset)
    for timepoint,pickup_type,drop_off_type in zip(route.pattern.timepoints,route.pattern.pickup_types,route.pattern.drop_off_types):
        attr = 0
        if timepoint == 1:
            attr |= 1
        if pickup_type != 1:
            attr |= 2
        if drop_off_type != 1:
            attr |= 4
        writebyte(attr)
    offset += 1 
route_stops_attributes_offsets.append(offset) # sentinel
assert len(route_stops_attributes_offsets) == nroutes + 1

# print db.service_ids()

def time_string_from_seconds(sec) :
    t = time.localtime(sec)
    #if t.tm_mday > 0 :
    return time.strftime('day %d %H:%M:%S', t)


print "saving a list of timedemandgroups"
write_text_comment("TIMEDEMANDGROUPS")
loc_timedemandgroups = tell()
offset = 0
timedemandgroups_offsets = {} # the offset into the stoptimes for each timedemandgroup ID
timedemandgroups_written = {}
timedemandgroup_t = Struct('HH')
n_nonincreasing_groups = 0
for idx, route in enumerate(route_for_idx) :
    for timedemandgroupref, times in route.gettimepatterns():
        assert len(times) == len(route.pattern.stop_ids)
        if str(times) in timedemandgroups_written:
            timedemandgroups_offsets[timedemandgroupref] = timedemandgroups_written[str(times)]
        else:
            timedemandgroups_offsets[timedemandgroupref] = offset
            timedemandgroups_written[str(times)] = offset
            for totaldrivetime, stopwaittime in times:
                out.write(timedemandgroup_t.pack(totaldrivetime >> 2, (totaldrivetime + stopwaittime) >> 2))
                offset += 1
            prev_time = None
            # coherency check: stoptimes should be increasing
            for time in times:
                if prev_time is not None :
                    arrive = time[0]
                    depart = time[0] + time[1]
                    prev_depart = prev_time[0] + prev_time[1]
                    if depart < arrive or arrive < prev_depart :
                        n_nonincreasing_groups += 1
                prev_time = time
print "%d time demand groups had non-increasing stoptimes" % (n_nonincreasing_groups,)
del(timedemandgroups_written)

print "saving a list of trips"
write_text_comment("TRIPS BY ROUTE")
loc_trips = tell()
toffset = 0
trips_offsets = []
trip_t = Struct('IHH') # Beware, Python structs do not have padding at the end.

all_trip_ids = []
trip_ids_offsets = [] # also serves as offsets into per-trip "service active" bitfields
tioffset = 0
for idx, route in enumerate(route_for_idx) :
    if idx > 0 and idx % 1000 == 0 :
        print 'wrote %d routes' % idx
        tell()
    # record the offset into the trip and trip_ids arrays for each trip block (route)
    trips_offsets.append(toffset)
    trip_ids_offsets.append(tioffset)
    trip_ids = route.sorted_trip_ids()
    # print idx, route, len(trip_ids)
    for timedemandgroupref, first_departure in db.fetch_timedemandgroups(trip_ids) :
        # 2**16 / 60 / 60 is only 18 hours
        # by right-shifting all times two bits we get 72 hours (3 days) at 4 second resolution
        # The last struct member is a realtime offset. The space is not wasted since it would be needed as struct padding anyway.
        out.write(trip_t.pack(timedemandgroups_offsets[timedemandgroupref], first_departure >> 2, 0))
        toffset += 1 
    all_trip_ids.extend(trip_ids)
    tioffset += len(trip_ids)
trips_offsets.append(toffset) # sentinel
trip_ids_offsets.append(tioffset) # sentinel
assert len(trips_offsets) == nroutes + 1
assert len(trip_ids_offsets) == nroutes + 1

print "writing trip attributes" 
write_text_comment("TRIP ATTRIBUTES")
loc_trip_attributes = tell()
for idx, route in enumerate(route_for_idx):
    for attributes in route.getattributes():
        trip_attr = 0
        if 'wheelchair_accessible' in attributes and attributes['wheelchair_accessible']:
            trip_attr |= 1
        writebyte(trip_attr)

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

print "saving transfer stops (footpaths)"
write_text_comment("TRANSFERS BY STOP")
loc_transfer_target_stops = tell()
offset = 0
transfers_offsets = []
for from_idx, from_sid in enumerate(stop_id_for_idx) :
    transfers_offsets.append(offset)
    for from_sid, to_sid, ttype, ttime in db.gettransfers(from_sid,maxdistance=MAX_DISTANCE):
        if ttime == None :
            continue # skip non-time/non-distance transfers for now
        to_idx = idx_for_stop_id[to_sid]
        writeint(to_idx)
        offset += 1
transfers_offsets.append(offset) # sentinel
assert len(transfers_offsets) == nstops + 1

print "saving transfer distances (footpaths)"
write_text_comment("TRANSFERS BY DISTANCE")
loc_transfer_dist_meters = tell()
offset = 0
transfers_offsets = []
for from_idx, from_sid in enumerate(stop_id_for_idx) :
    transfers_offsets.append(offset)
    for from_sid, to_sid, ttype, ttime in db.gettransfers(from_sid,maxdistance=MAX_DISTANCE):
        if ttime == None :
            continue # skip non-time/non-distance transfers for now
        to_idx = idx_for_stop_id[to_sid]
        # Store distances in units of 16 meters (rounding by adding 8)
        writebyte((int(ttime) + 8) >> 4)
        offset += 1
transfers_offsets.append(offset) # sentinel
assert len(transfers_offsets) == nstops + 1
                                       
print "saving stop indexes"
write_text_comment("STOP STRUCTS")
loc_stops = tell()
struct_2i = Struct('II')
for stop in zip (stop_routes_offsets, transfers_offsets) :
    out.write(struct_2i.pack(*stop));

print "saving stop attributes"
write_text_comment("STOP Attributes")
loc_stop_attributes = tell()
for stop_id,stop_name,stop_lat,stop_lon,attributes in db.stopattributes() :
    attr = 0
    if 'wheelchair_boarding' in attributes and attributes['wheelchair_boarding']:
        attr |= 1
    if 'visual_accessible' in attributes and attributes['visual_accessible']:
        attr |= 2
    writebyte(attr)

print "saving route indexes"
write_text_comment("ROUTE STRUCTS")
loc_routes = tell()
route_t = Struct('3I8H')
route_t_fields = [route_stops_offsets, trip_ids_offsets,headsign_offsets, route_n_stops, route_n_trips,route_attributes,agency_offsets,shortname_offsets,productcategory_offsets,route_min_time, route_max_time]
# check that all list lengths match the total number of routes. 
for l in route_t_fields :
    # the extra last route is a sentinel so we can derive list lengths for the last true route.
    assert len(l) == nroutes + 1
for route in zip (*route_t_fields) :
    # print route
    out.write(route_t.pack(*route));

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

print "writing out platformcodes for stops"
write_text_comment("PLATFORM CODES")
loc_platformcodes = write_string_table(platformcode_for_idx)

print "writing out stop names to string table"
write_text_comment("STOP NAME")
stop_names = sorted(nameloc_for_name.iteritems(), key=operator.itemgetter(1))
loc_stop_names = tell()
for stop_name,nameloc in stop_names:
    assert nameloc == out.tell() - loc_stop_names
    out.write(stop_name+'\0')    
out.write('\0')    

print "writing out locations for stopnames"
write_text_comment("STOP NAME LOCATIONS")
loc_stop_nameidx = tell()
for stop_name,nameloc in stop_names:
    writeint(nameloc)

print "writing out agencies to string table"
agencyIds = []
agencyNames = []
agencyUrls = []
sorted_agencyIds = sorted(idx_for_agency.iteritems(), key=operator.itemgetter(1))
for agency_id,agency_name,agency_url,agency_phone,agency_timezone in [db.agency(agencyId) for agencyId,idx in sorted_agencyIds]:
    agencyIds.append(agency_id)
    agencyNames.append(agency_name)
    agencyUrls.append(agency_url)
write_text_comment("AGENCY IDS")
print "writing out agencyIds to string table"
loc_agency_ids = write_string_table(agencyIds)
write_text_comment("AGENCY NAMES")
print "writing out agencyIds to string table"
loc_agency_names = write_string_table(agencyNames)
write_text_comment("AGENCY URLS")
loc_agency_urls = write_string_table(agencyUrls)

print "writing out headsigns to string table"
write_text_comment("HEADSIGNS")
sorted_headsigns = sorted(idx_for_headsign.iteritems(), key=operator.itemgetter(1))
loc_headsign = write_string_table([headsign for headsign,idx in sorted_headsigns])

print "writing out route_shortname's to string table"
write_text_comment("ROUTE SHORT NAMES")
sorted_routeshortnames = sorted(idx_for_shortname.iteritems(), key=operator.itemgetter(1))
loc_route_shortnames = write_string_table([shortname for shortname,idx in sorted_routeshortnames])

print "writing out productcategories to string table"
write_text_comment("PRODUCT CATEGORIES")
sorted_productcategories = sorted(idx_for_productcategory.iteritems(), key=operator.itemgetter(1))
loc_productcategories = write_string_table([productcategory for productcategory,idx in sorted_productcategories])

# maybe no need to store route IDs: report trip ids and look them up when reconstructing the response
print "writing route ids to string table"
write_text_comment("ROUTE IDS")
loc_route_ids = write_string_table(route_ids_for_idx)

print "writing out sorted stop ids to string table"
# stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
write_text_comment("STOP IDS")
loc_stop_ids = write_string_table(stop_id_for_idx)

print "writing trip ids to string table" 
# note that trip_ids are ordered by departure time within trip bundles (routes), which are themselves in arbitrary order. 
write_text_comment("TRIP IDS")
loc_trip_ids = write_string_table(all_trip_ids)

print "reached end of timetable file"
write_text_comment("END TTABLEV1")
loc_eof = tell()
print "rewinding and writing header... ",
write_header()
   
print "done."
out.close();

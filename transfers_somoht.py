#!/usr/bin/python

import math, sys    

# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase

verbose = False
RADIUS = 4000 # meters
OBSTRUCTION = 1.3 #factor to expand straight-line distance
range_lat = RADIUS / 111111.111

if len(sys.argv) < 2 :
    print 'usage: transfers.py infile.gtfsdb [verbose]'
    exit(1)
    
gtfsdb_file = sys.argv[1]
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file "%s" cannot be opened' % gtfsdb_file
    exit(1)
        
if len(sys.argv) > 2 and sys.argv[2] == "verbose" :
    verbose = True

# we we are interested in all routes available on each stop
all_query = """select stops.stop_id, stops.stop_name, stops.stop_lat, stops.stop_lon, routes from
(select stop_id, group_concat(route_id, ',') as routes 
 from (select distinct route_id, stop_id from trips, stop_times 
       where trips.trip_id = stop_times.trip_id) as x 
 group by stop_id) as y, stops where y.stop_id = stops.stop_id;"""
near_query = """
select stop_id, stop_name, stop_lat, stop_lon from stops where 
stop_lat > (:lat - :range_lat) and stop_lat < (:lat + :range_lat) and
stop_lon > (:lon - :range_lon) and stop_lon < (:lon + :range_lon) ;
"""
       
# equirectangular / sinusoidal projection
def distance (lat1, lon1, lat2, lon2) :
    avg_lat = (lat1 + lat2) / 2
    xscale = math.cos(math.radians(avg_lat))    
    return distance (lat1, lon1, lat2, lon2, xscale)
    
def distance (lat1, lon1, lat2, lon2, xscale) :
    dlon = lon2 - lon1
    dlat = lat2 - lat1
    dlon *= xscale
    d2 = dlon * dlon + dlat * dlat
    return math.sqrt(d2) * 111111.111

stops = {}

for sid, sname, lat, lon, routes in db.execute(all_query) :
    stops[sid] = {'name': sname, 'lat': lat, 'lon': lon, 'routes': set(routes.split(','))}

# can also compare squared distances in scaled meters
transfers = []
n_processed = 0
for sid, param in stops.items():
    if verbose :
        print sid, sname
    xscale = math.cos(math.radians(param['lat']))
    range_lon = range_lat * xscale
    # print xscale, range_lat, range_lon
    for sid2, sname2, lat2, lon2 in db.execute(near_query, {'range_lat': range_lat, 'range_lon': range_lon, 'lat': param['lat'], 'lon': param['lon']}):
        if sid == sid2:
            continue
        d = distance (param['lat'], param['lon'], lat2, lon2, xscale)
        if d > RADIUS :
            continue
        if verbose :
            print "  ", sid2, sname2, '%0.1f m' % d
        
        if sid2 in stops and len(stops[sid2]['routes'] - param['routes']) > 0:
            transfers.append ( (sid, sid2, d * OBSTRUCTION) )
    n_processed += 1;
    if n_processed % 1000 == 0 :
        print 'processed %d stops' % n_processed
 
cur = db.get_cursor()
cur.execute('delete from transfers;') # where transfer_type = 9;')
cur.executemany('insert into transfers values (?,?,9,?);', transfers)
cur.execute('create index if not exists transfers_from_stop_id ON transfers (from_stop_id)')
print 'committing...'
db.conn.commit()
# print 'vacuuming...'
# cur.execute('vacuum;')
# db.conn.commit()

    


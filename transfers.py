#!/usr/bin/python

import math

# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase

verbose = True
RADIUS = 400 # meters
OBSTRUCTION = 1.3 #factor to expand straight-line distance
FILE = '/home/abyrd/kv7.gtfsdb'
FILE = '/home/abyrd/trimet.gtfsdb'
db = GTFSDatabase(FILE)
range_lat = RADIUS / 111111.111

# a normal index on lat and lon is sufficiently fast, no need for a spatial index

all_query = "select stop_id, stop_name, stop_lat, stop_lon from stops;"
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
    
# can also compare squared distances in scaled meters
transfers = []
for sid, sname, lat, lon in db.execute(all_query) :
    if verbose :
        print sid, sname
    xscale = math.cos(math.radians(lat)) 
    range_lon = range_lat * xscale
    if verbose :
        print xscale, range_lat, range_lon
    for sid2, sname2, lat2, lon2 in db.execute(near_query, locals()):
        if sid2 == sid :
            continue
        d = distance (lat, lon, lat2, lon2, xscale)
        if d > RADIUS : 
            continue
        if verbose :
            print "  ", sid2, sname2, '%0.1f m' % d
        transfers.append ( (sid, sid2, d * OBSTRUCTION) )

cur = db.get_cursor()
cur.execute('delete from transfers where transfer_type = 9;')
cur.executemany('insert into transfers values (?,?,9,?);', transfers)
cur.execute('create index if not exists transfers_from_stop_id ON transfers (from_stop_id)')
db.conn.commit()
# print 'vacuuming...'
# cur.execute('vacuum;')
# db.conn.commit()

    


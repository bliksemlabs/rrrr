#!/usr/bin/python

import sys, struct

# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase

if len(sys.argv) != 2 :
    print 'usage: timetable.py inputfile.gtfsdb'
    exit(1)
else :    
    gtfsdb_file = sys.argv[1]
    
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file %s cannot be opened' % gtfsdb_file
    exit(1)

def stop_name(stop_id) :
    result = list(db.execute('select stop_name from stops where stop_id = ?', (stop_id,)))
    return result[0][0]
    
def trip_info(trip_id) :
    try :
        result = list(db.execute('select trip_id, trip_headsign from trips where trip_id = ?', (trip_id,)))[0]
        result = '%s richting %s' % result
    except :
        result = 'walk'
    return result
        
legs = []
for line in sys.stdin.readlines() :
    try :
        tripid, fromid, fromtime, toid, totime = line.split()
        line = '%s from %s at %8s to %s at %8s' % (trip_info(tripid), stop_name(fromid), fromtime, stop_name(toid), totime)
    except :
        pass
    legs.append(line)

while len(legs) > 0 :
    print legs.pop()
    print

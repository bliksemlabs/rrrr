#!/usr/bin/python

import sys, struct

# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase

if len(sys.argv) != 2 :
    print 'usage: datecheck.py inputfile.gtfsdb'
    exit(1)
else :    
    gtfsdb_file = sys.argv[1]
    
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file %s cannot be opened' % gtfsdb_file
    exit(1)

# check that all routes gs reports running are actually running on each day
for line in sys.stdin.readlines() :
    trip_id, fromid, fromtime, toid, totime = line.split()
    if trip_id == 'walk':
        continue
    service_id = list(db.execute('select service_id from trips where trip_id = ?', (trip_id,)))[0][0]
    print trip_id, '___', service_id
    # and date > 20130415 and date < 20130420 
    for line in db.execute('select date from calendar_dates where service_id = ? and date = 20130417 order by date', (service_id,)) :
        print line[0]        
    
    
    

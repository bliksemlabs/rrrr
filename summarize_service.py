#!/usr/bin/python

import sys, struct
from struct import Struct
# requires graphserver to be installed
from graphserver.ext.gtfs.gtfsdb import GTFSDatabase
from datetime import timedelta, date

gtfsdb_file = sys.argv[1]
try :
    with open(gtfsdb_file) as f :
        db = GTFSDatabase(gtfsdb_file)    
except IOError as e :
    print 'gtfsdb file %s cannot be opened' % gtfsdb_file
    exit(1)

# display number of active services to spot the usable period for this feed
dfrom, dto = db.date_range()
d = dfrom
while (d <= dto) :
    active_sids = db.service_periods(d)
    print d, len(active_sids)
    d += timedelta(days = 1)


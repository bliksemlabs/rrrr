#!/usr/bin/env python2
import rrrr
import sys
import os.path

if len(sys.argv) < 2 or not os.path.isfile(sys.argv[1]): 
    print ("Create a timetable from GTFS using rrtimetable.")
    sys.exit(-1)

router = rrrr.Raptor(timetable = sys.argv[1])

print router.stops()[0:10]
print router.route(from_id = 'vb', to_id = 'amf', depart = 1427883032)
print router.route(from_idx = 200, to_idx = 201, depart = 1427883032)
print router.route(from_latlon = (52.07, 4.35), to_id = 'amf', depart = 1427883032)

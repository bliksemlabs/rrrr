from model.transit import *
from gtfsdb import GTFSDatabase
import sys
from exporter.timetable3 import export
import exporter.timetable4
from datetime import timedelta, date

MAX_DAYS = 32

def convert(gtfsdb):

    from_date,to_date = gtfsdb.date_range()
    tdata = Timetable(from_date)
    print "Timetable valid from "+str(from_date)

    for stop_id,stop_name,stop_lat,stop_lon in gtfsdb.stop_areas():
        StopArea(tdata,stop_id,name=stop_name,latitude=stop_lat,longitude=stop_lon)

    for stop_id,stop_name,stop_lat,stop_lon,parent_station,platform_code in gtfsdb.stop_points():
        stop_area_uri = parent_station
        try:
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)
        except:
            stop_area_uri = 'StopArea:ZZ:'+stop_id
            StopArea(tdata,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon)
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)

    for from_stop_id,to_stop_id,min_transfer_time,transfer_type in gtfsdb.transfers():
        if from_stop_id == to_stop_id:
            continue
        if (int(min_transfer_time) >> 2) > 255:
            min_transfer_time = 255
        try:
            Connection(tdata,from_stop_id,to_stop_id,min_transfer_time,type=transfer_type)
            Connection(tdata,to_stop_id,from_stop_id,min_transfer_time,type=transfer_type)
        except:
            pass

    for from_stop_id,to_stop_id in gtfsdb.transfers_within_stoparea():
        min_transfer_time = 120
        try:
            Connection(tdata,from_stop_id,to_stop_id,min_transfer_time,type=transfer_type)
            Connection(tdata,to_stop_id,from_stop_id,min_transfer_time,type=transfer_type)
        except:
            pass

    for agency_id,agency_name,agency_url in gtfsdb.agencies():
        Operator(tdata,agency_id,name=agency_name,url=agency_url)

    for line_id,line_name,line_code,agency_id in gtfsdb.lines():
        if agency_id is None:
            if len(index.operators) == 1:
                agency_id = list(index.operators.keys())[0]
        Line(tdata,line_id,agency_id,name=line_name,code=line_code)

    for route_id,line_id,route_type in gtfsdb.routes():
        Route(tdata,route_id,line_id,route_type=route_type)

    calendars = {}
    for day_offset in range(MAX_DAYS) :
        date = tdata.validfrom + timedelta(days = day_offset)
        active_sids = gtfsdb.service_periods(date)
        for sid in active_sids:
            if sid not in calendars:
                calendars[sid] = []
            calendars[sid].append(date)
    
    vj = None
    last_trip_id = None
    for trip_id,service_id,route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type in gtfsdb.stop_times():
        if trip_id != last_trip_id:
            if vj is not None:
                vj.finish()
                vj = None
            if service_id not in calendars:
                continue
            last_trip_id = trip_id
            vj = VehicleJourney(tdata,trip_id,route_id,headsign=trip_headsign)
            for date in calendars[service_id]:
                vj.setIsValidOn(date)
        vj.add_stop(stop_id,arrival_time,departure_time,forboarding=(pickup_type != 1),foralighting=(drop_off_type != 1))
    if vj is not None:
        vj.finish()
    return tdata

from optparse import OptionParser

def main():
    parser = OptionParser()
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose", default=False, help="make a bunch of noise" )

    (options, args) = parser.parse_args()

    if len(args) < 1:
        print("Loads a GTFS file and generate a timetable suitable for RRRR.\nusage: gtfs2rrrr.py <input.gtfs.zip>")
        exit()
    
    gtfsdb_filename = args[0]+'.gtfsdb'
    gtfs_filename = args[0]
 
    gtfsdb = GTFSDatabase( gtfsdb_filename, overwrite=True )
    gtfsdb.load_gtfs( gtfs_filename, None, reporter=sys.stdout, verbose=options.verbose )
    tdata = convert(gtfsdb)
    if len(tdata.journey_patterns) == 0 or len(tdata.vehicle_journeys) == 0:
        print "No valid trips in this GTFS file!"
        sys.exit(1)
    export(tdata)
    exporter.timetable4.export(tdata)

if __name__=='__main__': 
    main()

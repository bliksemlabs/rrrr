from model.transit import *
from gtfsdb import GTFSDatabase
import sys
from exporter.timetable3 import export
import exporter.timetable4
from datetime import timedelta, date

MAX_DAYS = 32

def put_gtfs_modes(tdata):
    PhysicalMode(tdata,'0',name='Tram')
    PhysicalMode(tdata,'1',name='Metro')
    PhysicalMode(tdata,'2',name='Rail')
    PhysicalMode(tdata,'3',name='Bus')
    PhysicalMode(tdata,'4',name='Ferry')
    PhysicalMode(tdata,'5',name='Cable car')
    PhysicalMode(tdata,'6',name='Gondola')
    PhysicalMode(tdata,'7',name='Funicular')

    CommercialMode(tdata,'0',name='Tram')
    CommercialMode(tdata,'1',name='Metro')
    CommercialMode(tdata,'2',name='Rail')
    CommercialMode(tdata,'3',name='Bus')
    CommercialMode(tdata,'4',name='Ferry')
    CommercialMode(tdata,'5',name='Cable car')
    CommercialMode(tdata,'6',name='Gondola')
    CommercialMode(tdata,'7',name='Funicular')

def determine_timezone(gtfsdb):
    timezones = set([])
    for agency_id,agency_name,agency_url,agency_timezone in gtfsdb.agencies():
        if agency_timezone is not None:
            timezones.add(agency_timezone)

    if len(timezones) == 0:
        raise Exception("Agency has required field agency_timezone not filled in")
    elif len(timezones) > 1:
        raise Exception("Feed has multiple timezones, RRRR currently only supports one") #But should be trivial to change
    else:
        return list(timezones)[0]

def convert(gtfsdb, from_date=None):
    if from_date == None:
        from_date, _ = gtfsdb.date_range()

    feed_timezone = determine_timezone(gtfsdb)
    tdata = Timetable(from_date,feed_timezone)
    print "Timetable valid from %s to %s, timezone: %s" % (from_date, from_date + datetime.timedelta(days=MAX_DAYS),feed_timezone)
    
    put_gtfs_modes(tdata)

    for agency_id,agency_name,agency_url,agency_timezone in gtfsdb.agencies():
        Operator(tdata,agency_id,agency_timezone,name=agency_name,url=agency_url)

    for stop_id,stop_name,stop_lat,stop_lon,stop_timezone in gtfsdb.stop_areas():
        StopArea(tdata,stop_id,stop_timezone or feed_timezone,name=stop_name,latitude=stop_lat,longitude=stop_lon)

    for stop_id,stop_name,stop_lat,stop_lon,stop_timezone,parent_station,platform_code in gtfsdb.stop_points():
        stop_area_uri = parent_station
        try:
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)
        except:
            stop_area_uri = 'StopArea:ZZ:'+stop_id
            StopArea(tdata,stop_area_uri,stop_timezone or feed_timezone,name=stop_name,latitude=stop_lat,longitude=stop_lon)
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)

    for from_stop_id,to_stop_id,min_transfer_time,transfer_type in gtfsdb.transfers():
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

    for sp in tdata.stop_points.values():
        min_transfer_time = 120 #Create loop-transfers
        try:
            Connection(tdata,sp.uri,sp.uri,min_transfer_time,type=transfer_type)
        except:
            pass

    for line_id,line_name,line_code,agency_id,route_type,route_color,route_text_color in gtfsdb.lines():
        if agency_id is None:
            if len(index.operators) == 1:
                agency_id = list(index.operators.keys())[0]
        Line(tdata,line_id,agency_id,str(route_type),name=line_name,code=line_code,color=route_color,color_text=route_text_color)

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
    for trip_id,service_id,route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type,stop_headsign,route_type,block_id in gtfsdb.stop_times():
        if trip_id != last_trip_id:
            if vj is not None:
                vj.finish()
                vj = None
            if service_id not in calendars:
                continue
            last_trip_id = trip_id
            vj = VehicleJourney(tdata,trip_id,route_id,str(route_type),headsign=trip_headsign,blockref=block_id)
            for date in calendars[service_id]:
                vj.setIsValidOn(date)
        vj.add_stop(stop_id,arrival_time,departure_time,forboarding=(pickup_type != 1),foralighting=(drop_off_type != 1),headsign=stop_headsign)
    if vj is not None:
        vj.finish()
    return tdata

from optparse import OptionParser

def main():
    parser = OptionParser()
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose", default=False, help="make a bunch of noise" )
    parser.add_option("-d", "--from-date", action="store", type="string", dest="from_date", default=None, help="Explicitly set the first valid date of the timetable. Format: YYYY-MM-DD" )

    (options, args) = parser.parse_args()

    if options.from_date:
        from_date = datetime.datetime.strptime(options.from_date, '%Y-%m-%d').date()
    else:
        from_date = None

    if len(args) < 1:
        print("Loads a GTFS file and generate a timetable suitable for RRRR.\nusage: gtfs2rrrr.py <input.gtfs.zip>")
        exit()
    
    gtfsdb_filename = args[0]+'.gtfsdb'
    gtfs_filename = args[0]
 
    gtfsdb = GTFSDatabase( gtfsdb_filename, overwrite=True )
    gtfsdb.load_gtfs( gtfs_filename, None, reporter=sys.stdout, verbose=options.verbose )
    tdata = convert(gtfsdb, from_date)
    if len(tdata.journey_patterns) == 0 or len(tdata.vehicle_journeys) == 0:
        print "No valid trips in this GTFS file!"
        sys.exit(1)
    export(tdata)
    exporter.timetable4.export(tdata)

if __name__=='__main__': 
    main()

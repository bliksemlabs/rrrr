import psycopg2
from model.transit import *
import exporter.timetable4

def parse_gtfs_time(timestr):
    return (lambda x:int(x[0])*3600+int(x[1])*60+int(x[2]))(timestr.split(":")) #oh yes I did

def convert(dbname):
    conn = psycopg2.connect("dbname='ridprod'")

    cur = conn.cursor()
    cur.execute("SELECT DISTINCT agency_timezone FROM fusio.agency");
    feed_timezone = cur.fetchone()[0]
    cur.execute("SELECT MIN(date)::date FROM fusio.calendar_dates")
    tdata = Timetable(cur.fetchone()[0],feed_timezone)

    cur.execute("select physical_mode_id,physical_mode_name from fusio.physical_modes")
    for id,name in cur.fetchall():
        PhysicalMode(tdata,id,name=name)

    cur.execute("select commercial_mode_id,commercial_mode_name from fusio.commercial_modes")
    for id,name in cur.fetchall():
        CommercialMode(tdata,id,name=name)

    cur.execute("SELECT stop_id,stop_name,stop_lat,stop_lon,stop_timezone FROM fusio.stops WHERE location_type = 1")
    for stop_id,stop_name,stop_lat,stop_lon,stop_timezone in cur.fetchall():
        StopArea(tdata,stop_id,stop_timezone or feed_timezone,name=stop_name,latitude=stop_lat,longitude=stop_lon)
    cur.execute("SELECT stop_id,stop_name,stop_lat,stop_lon,stop_timezone,parent_station,platform_code FROM fusio.stops WHERE coalesce(location_type,0) = 0")
    for stop_id,stop_name,stop_lat,stop_lon,stop_timezone,parent_station,platform_code in cur.fetchall():
        stop_area_uri = parent_station
        try:
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)
        except:
            stop_area_uri = 'StopArea:ZZ:'+stop_id
            StopArea(tdata,stop_area_uri,stop_timezone or feed_timezone,name=stop_name,latitude=stop_lat,longitude=stop_lon)
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)
    cur.execute("SELECT from_stop_id,to_stop_id,min_transfer_time,transfer_type FROM fusio.transfers")
    for from_stop_id,to_stop_id,min_transfer_time,transfer_type in cur.fetchall():
        try:
            Connection(tdata,from_stop_id,to_stop_id,min_transfer_time,type=transfer_type)
            Connection(tdata,to_stop_id,from_stop_id,min_transfer_time,type=transfer_type)
        except:
            pass
    cur.execute("SELECT agency_id,agency_name,agency_url,agency_timezone FROM fusio.agency")
    for agency_id,agency_name,agency_url,agency_timezone in cur.fetchall():
        Operator(tdata,agency_id,agency_timezone,name=agency_name,url=agency_url)
    cur.execute("""
SELECT DISTINCT ON (line_id)
line_id,line_name,line_code,network_id,physical_mode_id,line_color
FROM fusio.lines JOIN fusio.routes USING (line_id) JOIN fusio.trips USING (route_id)
""")
    for line_id,line_name,line_code,network_id,physical_mode_id,line_color in cur.fetchall():
        Line(tdata,line_id,network_id,physical_mode_id,name=line_name,code=line_code,color=line_color)
    cur.execute("SELECT route_id,line_id,route_type FROM fusio.routes")
    for route_id,line_id,route_type in cur.fetchall():
        Route(tdata,route_id,line_id,route_type=route_type)

    calendars = {}
    cur.execute("SELECT service_id,array_agg(date::date) FROM fusio.calendar_dates GROUP BY service_id")
    for service_id,validdates in cur.fetchall():
        calendars[service_id] = validdates

    trip_id = None
    cur.close()
    cur = conn.cursor('trips')
    cur.execute("""
SELECT trip_id,service_id,route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type,stop_headsign,commercial_mode_id, block_id
FROM fusio.trips JOIN fusio.stop_times USING (trip_id) JOIN fusio.routes USING (route_id) JOIN fusio.lines USING (line_id)
ORDER BY trip_id,stop_sequence
""")
    vj = None
    last_trip_id = None
    for trip_id,service_id,route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type,stop_headsign,ccmode_id,block_id in cur:
        if trip_id != last_trip_id:
            if vj is not None:
                vj.finish()
            last_trip_id = trip_id
            vj = VehicleJourney(tdata,trip_id,route_id,ccmode_id,headsign=trip_headsign,blockref=block_id)
            for date in calendars[service_id]:
                vj.setIsValidOn(date)
        vj.add_stop(stop_id,parse_gtfs_time(arrival_time),parse_gtfs_time(departure_time),forboarding=(pickup_type != 1),foralighting=(drop_off_type != 1),headsign=stop_headsign)
    if vj is not None:
        vj.finish()
    return tdata
tdata = convert('ridprod')
export(tdata)
exporter.timetable4.export(tdata)

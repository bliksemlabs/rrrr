import psycopg2
from model.transit import *
from exporter.timetable3 import export

def parse_gtfs_time(timestr):
    return (lambda x:int(x[0])*3600+int(x[1])*60+int(x[2]))(timestr.split(":")) #oh yes I did

def convert(dbname):
    conn = psycopg2.connect("dbname='ridprod'")

    cur = conn.cursor()
    cur.execute("SELECT MIN(date)::date FROM fusio.calendar_dates")
    tdata = Timetable(cur.fetchone()[0])

    cur.execute("SELECT stop_id,stop_name,stop_lat,stop_lon FROM fusio.stops WHERE location_type = 1")
    for stop_id,stop_name,stop_lat,stop_lon in cur.fetchall():
        StopArea(tdata,stop_id,name=stop_name,latitude=stop_lat,longitude=stop_lon)
    cur.execute("SELECT stop_id,stop_name,stop_lat,stop_lon,parent_station,platform_code FROM fusio.stops WHERE coalesce(location_type,0) = 0")
    for stop_id,stop_name,stop_lat,stop_lon,parent_station,platform_code in cur.fetchall():
        stop_area_uri = parent_station
        try:
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)
        except:
            stop_area_uri = 'StopArea:ZZ:'+stop_id
            StopArea(tdata,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon)
            StopPoint(tdata,stop_id,stop_area_uri,name=stop_name,latitude=stop_lat,longitude=stop_lon,platformcode=platform_code)
    cur.execute("SELECT from_stop_id,to_stop_id,min_transfer_time,transfer_type FROM fusio.transfers")
    for from_stop_id,to_stop_id,min_transfer_time,transfer_type in cur.fetchall():
        if from_stop_id == to_stop_id:
            continue
        try:
            Connection(tdata,from_stop_id,to_stop_id,min_transfer_time,type=transfer_type)
            Connection(tdata,to_stop_id,from_stop_id,min_transfer_time,type=transfer_type)
        except:
            pass
    cur.execute("SELECT agency_id,agency_name,agency_url FROM fusio.agency")
    for agency_id,agency_name,agency_url in cur.fetchall():
        Operator(tdata,agency_id,name=agency_name,url=agency_url)
    cur.execute("SELECT line_id,line_name,line_code,network_id FROM fusio.lines")
    for line_id,line_name,line_code,network_id in cur.fetchall():
        Line(tdata,line_id,network_id,name=line_name,code=line_code)
    cur.execute("SELECT route_id,line_id,route_type FROM fusio.routes")
    for route_id,line_id,route_type in cur.fetchall():
        Route(tdata,route_id,line_id,route_type=route_type)

    calendars = {}
    cur.execute("SELECT service_id,array_agg(date::date) FROM fusio.calendar_dates GROUP BY service_id")
    for service_id,validdates in cur.fetchall():
        calendars[service_id] = validdates

    trip_id = None
    cur.execute("""
SELECT trip_id,service_id,route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type
FROM fusio.trips JOIN fusio.stop_times USING (trip_id)
ORDER BY trip_id,stop_sequence
""")
    vj = None
    last_trip_id = None
    for trip_id,service_id,route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type in cur.fetchall():
        if trip_id != last_trip_id:
            if vj is not None:
                vj.finish()
            last_trip_id = trip_id
            vj = VehicleJourney(tdata,trip_id,route_id,headsign=trip_headsign)
            for date in calendars[service_id]:
                vj.setIsValidOn(date)
        vj.add_stop(stop_id,parse_gtfs_time(arrival_time),parse_gtfs_time(departure_time),forboarding=(pickup_type != 1),foralighting=(drop_off_type != 1))
    if vj is not None:
        vj.finish()
    return tdata
tdata = convert('ridprod')
export(tdata)

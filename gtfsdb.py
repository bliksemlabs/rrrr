import csv
import sqlite3
import sys
import os
from zipfile import ZipFile
from codecs import iterdecode
import datetime

# Extended from gtfsdb.py, wich was part of the grapserver code.
#
# Unless otherwise noted, code included with Graphserver is covered under the BSD license

# Copyright (c) 2007, 2008, 2009, 2010, Graphserver contributors 

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:

# 	* Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.

#	* Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.

#	* Neither the name of the original author; nor the names of any contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.


# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


def withProgress(seq, modValue=100):
    c = -1

    for c, v in enumerate(seq):
        if (c+1) % modValue == 0: 
            sys.stdout.write("%s\r" % (c+1)) 
            sys.stdout.flush()
        yield v

    print("\nCompleted %s" % (c+1))

class UTF8TextFile(object):
    def __init__(self, fp):
        self.fp = fp
        
    def next(self):
        nextline = self.fp.next()
        return nextline.encode( "ascii", "ignore" )
        
    def __iter__(self):
        return self

def between(n, a, b):
    return n >= a and n<=b

def cons(ary):
    for i in range(len(ary)-1):
        yield (ary[i], ary[i+1])
        
def parse_gtfs_time(timestr):
    return (lambda x:int(x[0])*3600+int(x[1])*60+int(x[2]))(timestr.split(":")) #oh yes I did
    
def parse_gtfs_date(datestr):
    return (int(datestr[0:4]), int(datestr[4:6]), int(datestr[6:8]))

def create_table(cc, gtfs_basename, header):
    # Create stoptimes table
    sqlite_field_definitions = ["%s %s"%(field_name, field_type if field_type else "TEXT") for field_name, field_type, field_converter in header]
    cc.execute("create table %s (%s)"%(gtfs_basename,",".join(sqlite_field_definitions)))

def load_gtfs_table_to_sqlite(fp, gtfs_basename, cc, header=None, verbose=False):
    """header is iterable of (fieldname, fieldtype, processing_function). For example, (("stop_sequence", "INTEGER", int),). 
    "TEXT" is default fieldtype. Default processing_function is lambda x:x"""
    
    ur = UTF8TextFile( fp )
    rd = csv.reader( ur )

    # create map of field locations in gtfs header to field locations as specified by the table definition
    gtfs_header = [x.strip() for x in rd.next()]

    print(gtfs_header)
    
    gtfs_field_indices = dict(zip(gtfs_header, range(len(gtfs_header))))
    
    field_name_locations = [gtfs_field_indices[field_name] if field_name in gtfs_field_indices else None for field_name, field_type, field_converter in header]
    field_converters = [field_definition[2] for field_definition in header]
    field_operator = list(zip(field_name_locations, field_converters))

    # populate stoptimes table
    insert_template = 'insert into %s (%s) values (%s)'%(gtfs_basename,",".join([x[0] for x in header]), ",".join(["?"]*len(header)))
    print( insert_template )
    for i, line in withProgress(enumerate(rd), 5000):
        # carry on quietly if there's a blank line in the csv
        if line == []:
            continue
        
        _line = []
        for i, converter in field_operator:
            if i<len(line) and i is not None and line[i].strip() != "":
                if converter:
                    _line.append( converter(line[i].strip()) )
                else:
                    _line.append( line[i].strip() )
            else:
                _line.append( None )
                
        cc.execute(insert_template, _line)
        
class Pattern:
    def __init__(self, pattern_id, stop_ids):
        self.pattern_id = pattern_id
        self.stop_ids = stop_ids
    
    @property
    def signature(self):
        return (tuple(self.stops))

class TripBundle:
    def __init__(self, gtfsdb, pattern):
        self.gtfsdb = gtfsdb
        self.pattern = pattern
        self.trip_ids = []
        
    def add_trip(self, trip_id):
        self.trip_ids.append( trip_id )
        
    def stop_time_bundle( self, stop_id, service_id ):
        c = self.gtfsdb.conn.cursor()
        
        query = """
SELECT stop_times.* FROM stop_times, trips 
  WHERE stop_times.trip_id = trips.trip_id 
        AND trips.trip_id IN (%s) 
        AND trips.service_id = ? 
        AND stop_times.stop_id = ?
        AND arrival_time NOT NULL
        AND departure_time NOT NULL
  ORDER BY departure_time"""%(",".join(["'%s'"%x for x in self.trip_ids]))
      
        c.execute(query, (service_id,str(stop_id)))
        
        return list(c)
    
    def stop_time_bundles( self, service_id ):
        
        c = self.gtfsdb.conn.cursor()
        
        query = """
        SELECT stop_times.trip_id, 
               stop_times.arrival_time, 
               stop_times.departure_time, 
               stop_times.stop_id, 
               stop_times.stop_sequence, 
               stop_times.shape_dist_traveled 
        FROM stop_times, trips
        WHERE stop_times.trip_id = trips.trip_id
        AND trips.trip_id IN (%s)
        AND trips.service_id = ?
        AND arrival_time NOT NULL
        AND departure_time NOT NULL
        ORDER BY stop_sequence"""%(",".join(["'%s'"%x for x in self.trip_ids]))
            
        #bundle queries by trip_id
        
        trip_id_sorter = {}
        for trip_id, arrival_time, departure_time, stop_id, stop_sequence, shape_dist_traveled in c.execute(query, (service_id,)):
            if trip_id not in trip_id_sorter:
                trip_id_sorter[trip_id] = []
                
            trip_id_sorter[trip_id].append( (trip_id, arrival_time, departure_time, stop_id, stop_sequence, shape_dist_traveled) )
        
        return zip(*(trip_id_sorter.values()))
            
    def __repr__(self):
        return "<TripBundle n_trips: %d n_stops: %d>"%(len(self.trip_ids), len(self.pattern.stop_ids))

class GTFSDatabase:
    AGENCIES_DEF = ("agencies", (("agency_id",   None, None),
                                 ("agency_name",    None, None),
                                 ("agency_url", None, None),
                                 ("agency_timezone", None, None),
                                 ("agency_lang", None, None),
                                 ("agency_phone", None, None),
                                 ("agency_fare_url", None, None)))
    TRIPS_DEF = ("trips", (("route_id",   None, None),
                           ("trip_id",    None, None),
                           ("service_id", None, None),
                           ("shape_id", None, None),
                           ("trip_headsign", None, None),
                           ("trip_short_name", None, None),
                           ("direction_id", "INTEGER", None),
                           ("block_id", None, None),
                           ("trip_bikes_allowed", "INTEGER", None),
                           ("wheelchair_accessible", "INTEGER", None)))
    ROUTES_DEF = ("routes", (("agency_id", None, None),
                             ("route_id", None, None),
                             ("route_short_name", None, None),
                             ("route_long_name", None, None),
                             ("route_desc", None, None),
                             ("route_type", "INTEGER", None),
                             ("route_url", None, None),
                             ("route_color", None, None),
                             ("route_text_color", None, None)))
    STOP_TIMES_DEF = ("stop_times", (("trip_id", None, None), 
                                     ("arrival_time", "INTEGER", parse_gtfs_time),
                                     ("departure_time", "INTEGER", parse_gtfs_time),
                                     ("stop_id", None, None),
                                     ("stop_sequence", "INTEGER", None),
                                     ("stop_headsign", None, None),
                                     ("pickup_type", "INTEGER", None),
                                     ("drop_off_type", "INTEGER", None),
                                     ("shape_dist_traveled", "FLOAT", None)))
    STOPS_DEF = ("stops", (("stop_id", None, None),
                           ("stop_code", None, None),
                           ("stop_name", None, None),
                           ("stop_desc", None, None),
                           ("stop_lat", "FLOAT", None),
                           ("stop_lon", "FLOAT", None),
                           ("zone_id", None, None),
                           ("stop_url", None, None),
                           ("location_type", "INTEGER", None),
                           ("parent_station", None, None),
                           ("stop_timezone", None, None),
                           ("platform_code", None, None),
                           ("wheelchair_boarding", "INTEGER", None)))
    CALENDAR_DEF = ("calendar", (("service_id", None, None),
                                 ("monday", "INTEGER", None),
                                 ("tuesday", "INTEGER", None),
                                 ("wednesday", "INTEGER", None),
                                 ("thursday", "INTEGER", None),
                                 ("friday", "INTEGER", None),
                                 ("saturday", "INTEGER", None),
                                 ("sunday", "INTEGER", None),
                                 ("start_date", None, None),
                                 ("end_date", None, None)) )
    CAL_DATES_DEF = ("calendar_dates", (("service_id", None, None),
                                        ("date", None, None),
                                        ("exception_type", "INTEGER", None)) )
    AGENCY_DEF = ("agency", (("agency_id", None, None),
                             ("agency_name", None, None),
                             ("agency_url", None, None),
                             ("agency_timezone", None, None)) )
                             
    FREQUENCIES_DEF = ("frequencies", (("trip_id", None, None),
                                       ("start_time", "INTEGER", parse_gtfs_time),
                                       ("end_time", "INTEGER", parse_gtfs_time),
                                       ("headway_secs", "INTEGER", None)) )
    TRANSFERS_DEF = ("transfers", (("from_stop_id", None, None),    
                                       ("to_stop_id", None, None),
                                       ("from_route_id", None, None),
                                       ("to_route_id", None, None),
                                       ("from_trip_id", None, None),
                                       ("to_trip_id", None, None),
                                       ("transfer_type", "INTEGER", None),
                                       ("min_transfer_time", "FLOAT", None)))
    SHAPES_DEF = ("shapes", (("shape_id", None, None),
                               ("shape_pt_lat", "FLOAT", None),
                               ("shape_pt_lon", "FLOAT", None),
                               ("shape_pt_sequence", "INTEGER", None),
                               ("shape_dist_traveled", "FLOAT", None)))
    FEED_DEF = ("feed_info", (("feed_publisher_name", None, None),
                               ("feed_publisher_url", None, None),
                               ("feed_lang", None, None),
                               ("feed_start_date", None, None),
                               ("feed_end_date", None, None),
                               ("feed_version", None, None)))
    GTFS_DEF = (TRIPS_DEF, 
                STOP_TIMES_DEF, 
                STOPS_DEF, 
                CALENDAR_DEF, 
                CAL_DATES_DEF, 
                AGENCY_DEF, 
                FREQUENCIES_DEF, 
                ROUTES_DEF, 
                TRANSFERS_DEF,
                SHAPES_DEF,
                FEED_DEF)
    
    def __init__(self, sqlite_filename, overwrite=False):
        self.dbname = sqlite_filename
        
        if overwrite:
            try:
                os.remove(sqlite_filename)
            except:
                pass
        
        self.conn = sqlite3.connect( sqlite_filename )
        
    def get_cursor(self):
        # Attempts to get a cursor using the current connection to the db. If we've found ourselves in a different thread
        # than that which the connection was made in, re-make the connection.
        
        try:
            ret = self.conn.cursor()
        except sqlite3.ProgrammingError:
            self.conn = sqlite3.connect(self.dbname)
            ret = self.conn.cursor()
            
        return ret

    def load_gtfs(self, gtfs_filename, tables=None, reporter=None, verbose=False):
        c = self.get_cursor()

        if not os.path.isdir( gtfs_filename ):
            zf = ZipFile( gtfs_filename )

        for tablename, table_def in self.GTFS_DEF:
            if tables is not None and tablename not in tables:
                print( "skipping table %s - not included in 'tables' list"%tablename )
                continue

            print( "creating table %s\n"%tablename )
            create_table( c, tablename, table_def )
            print( "loading table %s\n"%tablename )
            
            try:
                if not os.path.isdir( gtfs_filename ):
                    trips_file = iterdecode( zf.read(tablename+".txt").split("\n"), "utf-8" )
                else:
                    trips_file = iterdecode( open( os.path.join( gtfs_filename, tablename+".txt" ) ), "utf-8" )
                load_gtfs_table_to_sqlite(trips_file, tablename, c, table_def, verbose=verbose)
            except (KeyError, IOError):
                print( "NOTICE: GTFS feed has no file %s.txt, cannot load\n"%tablename )
    
        self._create_indices(c)
        self.conn.commit()
        c.close()

    def _create_indices(self, c):
        
        c.execute( "CREATE INDEX stop_times_trip_id ON stop_times (trip_id)" )
        c.execute( "CREATE INDEX stop_times_stop_id ON stop_times (stop_id)" )
        c.execute( "CREATE INDEX trips_trip_id ON trips (trip_id)" )
        c.execute( "CREATE INDEX stops_stop_lat ON stops (stop_lat)" )
        c.execute( "CREATE INDEX stops_stop_lon ON stops (stop_lon)" )
        c.execute( "CREATE INDEX route_route_id ON routes (route_id)" )
        c.execute( "CREATE INDEX trips_route_id ON trips (route_id)" )
        c.execute( "CREATE INDEX transfers_fromstop_id ON transfers (from_stop_id)" )
        c.execute( "CREATE INDEX transfers_tostop_id ON transfers (to_stop_id)" )

    def stops(self):
        c = self.get_cursor()
        
        c.execute( "SELECT * FROM stops" )
        ret = list(c)
        
        c.close()
        return ret
        
    def stop(self, stop_id):
        c = self.get_cursor()
        c.execute( "SELECT * FROM stops WHERE stop_id = ?", (stop_id,) )
        ret = c.next()
        c.close()
        return ret
        
    def count_stops(self):
        c = self.get_cursor()
        c.execute( "SELECT count(*) FROM stops" )
        
        ret = c.next()[0]
        c.close()
        return ret

    def compile_trip_bundles(self, maxtrips=None, reporter=None):
        
        c = self.get_cursor()

        patterns = {}
        bundles = {}

        c.execute( "SELECT count(*) FROM trips" )
        n_trips = c.next()[0]
        
        if maxtrips is not None and maxtrips < n_trips:
            n_trips = maxtrips;

        if maxtrips is not None:
            c.execute( "SELECT trip_id FROM trips LIMIT ?", (maxtrips,) )
        else:
            c.execute( "SELECT trip_id FROM trips" )
            
        for i, (trip_id,) in enumerate(c):
            if reporter and i%(n_trips//50+1)==0: reporter.write( "%d/%d trips grouped by %d patterns\n"%(i,n_trips,len(bundles)))
            
            d = self.get_cursor()
            d.execute( "SELECT trip_id, arrival_time, departure_time, stop_id,route_id FROM stop_times LEFT JOIN trips using (trip_id) WHERE trip_id = ? AND arrival_time NOT NULL AND departure_time NOT NULL ORDER BY stop_sequence", (trip_id,) )
            
            stop_times = list(d)
            
            stop_ids = [stop_id for trip_id, arrival_time, departure_time, stop_id,route_id in stop_times]
            route_id = [route_id for trip_id, arrival_time, departure_time, stop_id,route_id in stop_times][0]
            pattern_signature = (tuple(stop_ids), route_id)
            
            if pattern_signature not in patterns:
                pattern = Pattern( len(patterns), stop_ids)
                patterns[pattern_signature] = pattern
            else:
                pattern = patterns[pattern_signature]
                
            if pattern not in bundles:
                bundles[pattern] = TripBundle( self, pattern )
            
            bundles[pattern].add_trip( trip_id )

        c.close()
        
        return bundles.values()
        
    def nearby_stops(self, lat, lng, range):
        c = self.get_cursor()
        
        c.execute( "SELECT * FROM stops WHERE stop_lat BETWEEN ? AND ? AND stop_lon BETWEEN ? And ?", (lat-range, lat+range, lng-range, lng+range) )
        
        for row in c:
            yield row

    def extent(self):
        c = self.get_cursor()
        
        c.execute( "SELECT min(stop_lon), min(stop_lat), max(stop_lon), max(stop_lat) FROM stops" )
        
        ret = c.next()
        c.close()
        return ret
        
    def execute(self, query, args=None):
        
        c = self.get_cursor()
        
        if args:
            c.execute( query, args )
        else:
            c.execute( query )
            
        for record in c:
            yield record
        c.close()
        
    def agency_timezone_name(self, agency_id_or_name=None):

        if agency_id_or_name is None:
            agency_timezone_name = list(self.execute( "SELECT agency_timezone FROM agency LIMIT 1" ))
        else:
            agency_timezone_name = list(self.execute( "SELECT agency_timezone FROM agency WHERE agency_id=? OR agency_name=?", (agency_id_or_name,agency_id_or_name) ))
        
        return agency_timezone_name[0][0]
        
    def day_bounds(self):
        daymin = list( self.execute("select min(departure_time) from stop_times") )[0][0]
        daymax = list( self.execute("select max(arrival_time) from stop_times") )[0][0]
        
        return (daymin, daymax)
        
    def date_range(self):
        start_date, end_date = list( self.execute("select min(start_date), max(end_date) from calendar") )[0]
        
        start_date = start_date or "99999999" #sorted greater than any date
        end_date = end_date or "00000000" #sorted earlier than any date
        
        first_exception_date, last_exception_date = list( self.execute("select min(date), max(date) from calendar_dates WHERE exception_type=1") )[0]
          
        first_exception_date = first_exception_date or "99999999"
        last_exceptoin_date = last_exception_date or "00000000"
        
        start_date = min(start_date, first_exception_date)
        end_date = max(end_date, last_exception_date )

        return datetime.date( *parse_gtfs_date(start_date) ), datetime.date( *parse_gtfs_date(end_date) )
    
    DOWS = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday']
    DOW_INDEX = dict(zip(range(len(DOWS)),DOWS))
    
    def service_periods(self, sample_date):
        datetimestr = sample_date.strftime( "%Y%m%d" ) #sample_date to string like "20081225"
        datetimeint = int(datetimestr)              #int like 20081225. These ints have the same ordering as regular dates, so comparison operators work
        
        # Get the gtfs date range. If the sample_date is out of the range, no service periods are in effect
        start_date, end_date = self.date_range()
        if sample_date < start_date or sample_date > end_date:
            return []
        
        # Use the day-of-week name to query for all service periods that run on that day
        dow_name = self.DOW_INDEX[sample_date.weekday()]
        service_periods = list( self.execute( "SELECT service_id, start_date, end_date FROM calendar WHERE %s=1"%dow_name ) )
         
        # Exclude service periods whose range does not include this sample_date
        service_periods = [x for x in service_periods if (int(x[1]) <= datetimeint and int(x[2]) >= datetimeint)]
        
        # Cut service periods down to service IDs
        sids = set( [x[0] for x in service_periods] )
            
        # For each exception on the given sample_date, add or remove service_id to the accumulating list
        
        for exception_sid, exception_type in self.execute( "select service_id, exception_type from calendar_dates WHERE date = ?", (datetimestr,) ):
            if exception_type == 1:
                sids.add( exception_sid )
            elif exception_type == 2:
                if exception_sid in sids:
                    sids.remove( exception_sid )
                
        return list(sids)
        
    def service_ids(self):
        query = "SELECT DISTINCT service_id FROM (SELECT service_id FROM calendar UNION SELECT service_id FROM calendar_dates)"
        
        return [x[0] for x in self.execute( query )]
    
    def shape(self, shape_id):
        query = "SELECT shape_pt_lon, shape_pt_lat, shape_dist_traveled from shapes where shape_id = ? order by shape_pt_sequence"
        
        return list(self.execute( query, (shape_id,) ))
    
    def shape_from_stops(self, trip_id, stop_sequence1, stop_sequence2):
        query = """SELECT stops.stop_lon, stop_lat 
                   FROM stop_times as st, stops 
                   WHERE trip_id=? and st.stop_id=stops.stop_id and stop_sequence between ? and ? 
                   ORDER by stop_sequence"""
                   
        return list(self.execute( query, (trip_id, stop_sequence1, stop_sequence2) ))
    
    def shape_between(self, trip_id, stop_sequence1, stop_sequence2):
        # get shape_id of trip
        shape_id = list(self.execute( "SELECT shape_id FROM trips WHERE trip_id=?", (trip_id,) ))[0][0]
        
        if shape_id is None:
            return self.shape_from_stops( trip_id, stop_sequence1, stop_sequence2 )
        
        query = """SELECT min(shape_dist_traveled), max(shape_dist_traveled)
                     FROM stop_times
                     WHERE trip_id=? and (stop_sequence = ? or stop_sequence = ?)"""
        t_min, t_max = list(self.execute( query, (trip_id, stop_sequence1, stop_sequence2) ))[0]
        
        if t_min is None or \
           ( hasattr(t_min,"strip") and t_min.strip()=="" ) or \
           t_max is None or \
           ( hasattr(t_max,"strip") and t_max.strip()=="" ) :
            return self.shape_from_stops( trip_id, stop_sequence1, stop_sequence2 )
                
        ret = []
        for (lon1, lat1, dist1), (lon2, lat2, dist2) in cons(self.shape(shape_id)):
            if between( t_min, dist1, dist2 ):
                percent_along = (t_min-dist1)/float((dist2-dist1)) if dist2!=dist1 else 0
                lat = lat1+percent_along*(lat2-lat1)
                lon = lon1+percent_along*(lon2-lon1)
                ret.append( (lon, lat) )

            if between( dist2, t_min, t_max ):
                ret.append( (lon2, lat2) )
                
            if between( t_max, dist1, dist2):
                percent_along = (t_max-dist1)/float((dist2-dist1)) if dist2!=dist1 else 0
                lat = lat1+percent_along*(lat2-lat1)
                lon = lon1+percent_along*(lon2-lon1)
                ret.append( (lon, lat) )
                
        return ret
                

def main_inspect_gtfsdb():
    from sys import argv
    
    if len(argv) < 2:
        print("usage: python gtfsdb.py gtfsdb_filename [query]")
        exit()
    
    gtfsdb_filename = argv[1]
    gtfsdb = GTFSDatabase( gtfsdb_filename )
    
    if len(argv) == 2:
        for table_name, fields in gtfsdb.GTFS_DEF:
            print("Table: %s"%table_name)
            for field_name, field_type, field_converter in fields:
                print("\t%s %s"%(field_type, field_name))
        exit()
    
    query = argv[2]
    for record in gtfsdb.execute( query ):
        print(record)
    
    #for stop_id, stop_name, stop_lat, stop_lon in gtfsdb.stops():
    #    print( stop_lat, stop_lon )
    #    gtfsdb.nearby_stops( stop_lat, stop_lon, 0.05 )
    #    break
    
    #bundles = gtfsdb.compile_trip_bundles()
    #for bundle in bundles:
    #    for departure_set in bundle.iter_departures("WKDY"):
    #        print( departure_set )
    #    
    #    #print( len(bundle.trip_ids) )
    #    sys.stdout.flush()

    pass

from optparse import OptionParser

def main_compile_gtfsdb():
    parser = OptionParser()
    parser.add_option("-t", "--table", dest="tables", action="append", default=[], help="copy over only the given tables")
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose", default=False, help="make a bunch of noise" )

    (options, args) = parser.parse_args()
    if len(options.tables)==0:
        options.tables=None

    if len(args) < 2:
        print("Converts GTFS file to GTFS-DB, which is super handy\nusage: python process_gtfs.py gtfs_filename gtfsdb_filename")
        exit()
    
    gtfsdb_filename = args[1]
    gtfs_filename = args[0]
 
    gtfsdb = GTFSDatabase( gtfsdb_filename, overwrite=True )
    gtfsdb.load_gtfs( gtfs_filename, options.tables, reporter=sys.stdout, verbose=options.verbose )


if __name__=='__main__': 
    main_compile_gtfsdb()

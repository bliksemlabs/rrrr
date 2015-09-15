#!/usr/bin/env python2

import csv
import sqlite3
import sys
import os
from zipfile import ZipFile
from codecs import iterdecode
from datetime import timedelta,datetime,date

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
        return nextline.encode( "UTF-8" )

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
    def insert_data():
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

            yield _line

    cc.executemany(insert_template, insert_data())

class GTFSDatabase:
    TRIPS_DEF = ("trips", (("route_id",   None, None),
                           ("trip_id",    None, None),
                           ("realtime_trip_id",    None, None),
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
                                     ("timepoint", "INTEGER", None),
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
                             ("agency_phone", None, None),
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
        self.conn.text_factory = str

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

    def date_range(self):
        start_date, end_date = list( self.get_cursor().execute("select min(start_date), max(end_date) from calendar") )[0]

        start_date = start_date or "99999999" #sorted greater than any date
        end_date = end_date or "00000000" #sorted earlier than any date

        first_exception_date, last_exception_date = list( self.get_cursor().execute("select min(date), max(date) from calendar_dates WHERE exception_type=1") )[0]

        first_exception_date = first_exception_date or "99999999"
        last_exceptoin_date = last_exception_date or "00000000"

        start_date = min(start_date, first_exception_date)
        end_date = max(end_date, last_exception_date )

        return date( *parse_gtfs_date(start_date) ), date( *parse_gtfs_date(end_date) )

    def stop_points(self):
        c = self.get_cursor()
        c.execute( "SELECT stop_id,stop_name,stop_lat,stop_lon,stop_timezone,parent_station,platform_code FROM stops WHERE coalesce(location_type,0) = 0 ORDER BY stop_id " )
        ret = list(c)
        c.close()
        return ret

    def stop_areas(self):
        c = self.get_cursor()
        c.execute( "SELECT stop_id,stop_name,stop_lat,stop_lon,stop_timezone FROM stops WHERE location_type = 1 ORDER BY stop_id " )
        ret = list(c)
        c.close()
        return ret

    def transfers(self):
        c = self.get_cursor()
        c.execute( "SELECT from_stop_id,to_stop_id,min_transfer_time,transfer_type FROM transfers" )
        ret = list(c)
        c.close()
        return ret

    def transfers_within_stoparea(self):
        c = self.get_cursor()
        c.execute("""
SELECT s1.stop_id,s2.stop_id
FROM stops s1 JOIN stops s2 USING (parent_station)
WHERE s1.parent_station is not null AND s1.stop_id != s2.stop_id
""" )
        ret = list(c)
        c.close()
        return ret

    def agencies(self):
        c = self.get_cursor()
        c.execute( "SELECT agency_id,agency_name,agency_url,agency_timezone FROM agency" )
        ret = list(c)
        c.close()
        return ret

    def lines(self):
        c = self.get_cursor()
        c.execute( "SELECT route_id as line_id, route_long_name as line_name, coalesce(route_short_name,route_long_name) as line_code, agency_id,route_type,route_color,route_text_color FROM routes" )
        ret = list(c)
        c.close()
        return ret

    def stop_times(self):
        c = self.get_cursor()
        c.execute( """
SELECT trip_id,realtime_trip_id,service_id,route_id||':'||coalesce(direction_id,0) as route_id,trip_headsign,stop_sequence,stop_id,arrival_time,departure_time,pickup_type,drop_off_type,stop_headsign,route_type,block_id
FROM trips JOIN stop_times USING (trip_id) JOIN routes USING (route_id)
ORDER BY trip_id,stop_sequence
""")
        ret = list(c)
        c.close()
        return ret

    def routes(self):
        c = self.get_cursor()
        c.execute( """
SELECT DISTINCT route_id||':'||coalesce(direction_id,0) as route_id,route_id as line_id,route_type
FROM trips JOIN routes USING (route_id)
""")
        ret = list(c)
        c.close()
        return ret

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
        service_periods = list( self.get_cursor().execute( "SELECT service_id, start_date, end_date FROM calendar WHERE %s=1"%dow_name ) )

        # Exclude service periods whose range does not include this sample_date
        service_periods = [x for x in service_periods if (int(x[1]) <= datetimeint and int(x[2]) >= datetimeint)]

        # Cut service periods down to service IDs
        sids = set( [x[0] for x in service_periods] )

        # For each exception on the given sample_date, add or remove service_id to the accumulating list

        for exception_sid, exception_type in self.get_cursor().execute( "select service_id, exception_type from calendar_dates WHERE date = ?", (datetimestr,) ):
            if exception_type == 1:
                sids.add( exception_sid )
            elif exception_type == 2:
                if exception_sid in sids:
                    sids.remove( exception_sid )

        return list(sids)


from optparse import OptionParser

def main_compile_gtfsdb():
    parser = OptionParser()
    parser.add_option("-t", "--table", dest="tables", action="append", default=[], help="copy over only the given tables")
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose", default=False, help="make a bunch of noise" )

    (options, args) = parser.parse_args()
    if len(options.tables)==0:
        options.tables=None

    if len(args) < 2:
        print("Loads a GTFS file into an SQLite database, enabling more sophisticated queries.\nusage: gtfsdb.py <input.gtfs.zip> <output.gtfsdb>")
        exit()

    gtfsdb_filename = args[1]
    gtfs_filename = args[0]

    gtfsdb = GTFSDatabase( gtfsdb_filename, overwrite=True )
    gtfsdb.load_gtfs( gtfs_filename, options.tables, reporter=sys.stdout, verbose=options.verbose )
    print "Done loading GTFS into database. Don't forget to add transfers to the database if needed!"

if __name__=='__main__':
    main_compile_gtfsdb()


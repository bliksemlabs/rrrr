import datetime
import dateutil
import pytz

def validate_timezone(timezone):
    return timezone is not None and dateutil.tz.gettz(timezone) is not None

timezone_cache = {}
resp_cache = {}

def utc_time_gtfs(validdate,time,timezone_id):
     if timezone_id not in timezone_cache:
         timezone_cache[timezone_id] = pytz.timezone(timezone_id)
     timezone = timezone_cache[timezone_id]
     dt = datetime.datetime.combine(validdate,datetime.time(12,0,0))
     utc_offset = timezone.utcoffset(dt).seconds
     return (utc_offset,time-utc_offset)

import datetime
import dateutil.tz

class Timetable:
    def __init__(self,validfrom):
        if not isinstance(validfrom, datetime.date):
            raise TypeError('validfrom must be a datetime.date, not a %s' % type(validfrom))
        self.validfrom = validfrom
        self.stop_areas = {}
        self.stop_points = {}
        self.operators = {}
        self.routes = {}
        self.lines = {}
        self.vehicle_journeys = {}
        self.connections = {}
        self.journey_patterns = {}
        self.signature_to_jp = {}
        self.timedemandgroups = {}
        self.signature_to_timedemandgroup = {}
        self.physical_modes = {}
        self.commercial_modes = {}

class StopArea:

    def validate_timezone(self,timezone):
        return timezone is not None and dateutil.tz.gettz(timezone) is not None

    def __init__(self,timetable,uri,timezone,name=None,latitude=None,longitude=None):
        self.type = 'stop_area'
        self.uri = uri
        if uri in timetable.stop_areas:
            raise ValueError('Violation of unique StopArea key') 
        timetable.stop_areas[uri] = self
        if not self.validate_timezone(timezone):
            raise Exception("Invalid timezone")
        self.timezone = timezone
        self.name = name
        self.latitude = latitude
        self.longitude = longitude

class CommercialMode:
    def __init__(self,timetable,uri,name=None):
        self.type = 'commercial_mode'
        self.uri = uri
        if uri in timetable.commercial_modes:
            raise ValueError('Violation of unique CommercialMode key') 
        timetable.commercial_modes[uri] = self
        self.name = name

    def __hash__(self):
        return hash(freeze(self.__dict__))

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.__dict__ == other.__dict__
        else:
            return False

class PhysicalMode:
    def __init__(self,timetable,uri,name=None):
        self.type = 'physical_mode'
        self.uri = uri
        if uri in timetable.physical_modes:
            raise ValueError('Violation of unique PhysicalMode key') 
        timetable.physical_modes[uri] = self
        self.name = name

    def __hash__(self):
        return hash(freeze(self.__dict__))

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.__dict__ == other.__dict__
        else:
            return False

class StopPoint:
    def __init__(self,timetable,uri,stop_area_uri,name=None,platformcode=None,latitude=None,longitude=None):
        self.type = 'stop_point'
        self.uri = uri
        if stop_area_uri not in timetable.stop_areas:
            raise ValueError('Violation of foreign key, stop_area not found') 
        self.stop_area = timetable.stop_areas[stop_area_uri]
        if uri in timetable.stop_points:
            raise ValueError('Violation of unique StopPoint key') 
        timetable.stop_points[uri] = self
        self.name = name
        self.platformcode = platformcode
        self.latitude = latitude
        self.longitude = longitude

class Connection:
    def __init__(self,timetable,from_stop_point_uri,to_stop_point_uri,min_transfer_time,type=None,):
        self.type = 'connection'
        if from_stop_point_uri not in timetable.stop_points:
            raise ValueError('Violation of foreign key, from_stop_point not found') 
        if to_stop_point_uri not in timetable.stop_points:
            raise ValueError('Violation of foreign key, to_stop_point not found') 
        if (from_stop_point_uri,to_stop_point_uri) in timetable.connections:
            raise ValueError('Violation of unique key, unique connections between stop_points required')
        self.from_stop_point = timetable.stop_points[from_stop_point_uri]
        self.to_stop_point = timetable.stop_points[to_stop_point_uri]
        self.min_transfer_time = min_transfer_time
        self.type = type
        timetable.connections[(from_stop_point_uri,to_stop_point_uri)] = self

class Operator:
    def __init__(self,timetable,uri,name=None,url=None):
        self.type = 'operator'
        self.uri = uri
        if uri in timetable.operators:
            raise ValueError('Violation of unique Operator key') 
        timetable.operators[uri] = self
        self.name = name
        self.url = url

class Line:
    def __init__(self,timetable,uri,operator_uri,physical_mode_uri,name=None,code=None):
        self.type = 'line'
        self.uri = uri
        if operator_uri not in timetable.operators:
            raise ValueError('Violation of foreign key, operator not found')
        self.operator = timetable.operators[operator_uri]
        if physical_mode_uri not in timetable.physical_modes:
            raise ValueError('Violation of foreign key, physical_mode not found')
        self.physical_mode = timetable.physical_modes[physical_mode_uri]
        if uri in timetable.lines:
            raise ValueError('Violation of unique Line key') 
        timetable.lines[uri] = self
        self.name = name
        self.code = code

class Route:
    def __init__(self,timetable,uri,line_uri,direction=None,route_type=None):
        self.type = 'route'
        self.uri = uri
        self.journey_patterns = []
        if line_uri not in timetable.lines:
            raise ValueError('Violation of foreign key, line not found')
        self.line = timetable.lines[line_uri]
        self.route_type = route_type
        if uri in timetable.routes:
            raise ValueError('Violation of unique Route key') 
        timetable.routes[uri] = self
        if direction is not None:
            self.direction = direction

def freeze(o):
  if isinstance(o,dict):
    return frozenset({ k:freeze(v) for k,v in o.items()}.items())
  if isinstance(o,list):
    return tuple([freeze(v) for v in o])
  return o

class JourneyPatternPoint:
    def __init__(self,timetable,stop_point_uri,forboarding=True,foralighting=True,timingpoint=False,headsign=None):
        if stop_point_uri not in timetable.stop_points:
            raise ValueError('Violation of foreign key, StopPoint not found')
        self.type = 'journey_pattern_point'
        self.stop_point = timetable.stop_points[stop_point_uri]
        self.forboarding = forboarding
        self.foralighting = foralighting
        self.timingpoint = timingpoint
        self.headsign = headsign

    def __hash__(self):
        return hash(freeze(self.__dict__))

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.__dict__ == other.__dict__
        else:
            return False

class TimeDemandGroupPoint:
    def __init__(self,drivetime,totaldrivetime):
        self.type = 'timedemandgrouppoint'
        self.drivetime = drivetime
        self.totaldrivetime = totaldrivetime

    def __hash__(self):
        return hash(freeze(self.__dict__))

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.__dict__ == other.__dict__
        else:
            return False

class TimeDemandGroup:
    def __init__(self,timetable,uri,points):
        self.type = 'timedemandgroup'
        self.uri = uri
        self.points = points

class JourneyPattern:
    def __init__(self,timetable,uri,route_uri,points,headsign=None,commercial_mode=None):
        if route_uri not in timetable.routes:
            raise ValueError('Violation of foreign key, route not found')
        self.route = timetable.routes[route_uri]
        self.type = 'journey_pattern'
        self.uri = uri
        self.points = points
        self.commercial_mode = commercial_mode
        self.headsign = headsign

    def __hash__(self):
        return hash(freeze(self.__dict__))

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.__dict__ == other.__dict__
        else:
            return False

class VehicleJourney:
    def __init__(self,timetable,uri,route_uri,commercial_mode_uri,headsign=None):
        self.timetable = timetable
        self.type = 'vehicle_journey'
        self.uri = uri
        self.headsign = headsign
        self.validity_pattern = set([])
        if uri in timetable.vehicle_journeys:
            raise ValueError('Violation of unique VehicleJourney key') 
        timetable.vehicle_journeys[uri] = self
        if route_uri not in timetable.routes:
            raise ValueError('Violation of foreign key, route not found')
        if commercial_mode_uri not in timetable.commercial_modes:
            raise ValueError('Violation of foreign key, commercial_mode not found')
        self.commercial_mode = timetable.commercial_modes[commercial_mode_uri]
        self.route = timetable.routes[route_uri]
        self.points = []
        self.timedemandgroup = []
        self.__isfinished__ = False

    def setIsValidOn(self,validdate):
        if not isinstance(validdate, datetime.date):
            raise TypeError('validfrom must be a datetime.date, not a %s' % type(validdate))
        if validdate < self.timetable.validfrom:
            return False
        self.validity_pattern.add((validdate-self.timetable.validfrom).days)
        return True

    def add_stop(self,stop_point_uri,arrival_time,departure_time,forboarding=True,foralighting=True,timingpoint=False,headsign=None):
        if self.__isfinished__:
            raise AttributeError('VehicleJourney was previously completed')
        if stop_point_uri not in self.timetable.stop_points:
            raise ValueError('Violation of foreign key, StopPoint not found')
        if arrival_time < 0 or departure_time < 0:
            raise ValueError('Negative times')
        if arrival_time > departure_time:
            raise ValueError('Negative dwelltime')
        if len(self.points) == 0:
            self.departure_time = departure_time
            self.timedemandgroup.append(TimeDemandGroupPoint(0,0))
            foralighting = False # Do not allow alighting at first stop
        else:
            drivetime,totaldrivetime = (arrival_time-self.departure_time,departure_time-self.departure_time)
            if drivetime < self.timedemandgroup[-1].totaldrivetime:
                raise ValueError('Timetravel from latest stop')
            self.timedemandgroup.append(TimeDemandGroupPoint(drivetime,totaldrivetime))
        self.points.append(JourneyPatternPoint(self.timetable,stop_point_uri,forboarding,foralighting,timingpoint,headsign=(headsign or self.headsign)))

    def finish(self):
        if self.__isfinished__:
            raise AttributeError('VehicleJourney was previously completed')
        self.__isfinished__ = True
        if len(self.points) != len(self.timedemandgroup):
            raise ValueError('Length of timedemandgroup is not equal with journeypattern')
        if len(self.points) < 2:
            raise ValueError('Number of stops is lower than 2')
        self.timedemandgroup = tuple(self.timedemandgroup)
        self.points[-1].forboarding = False #Do not allow boarding at final stop
        self.points = tuple(self.points)
        pattern_signature = (self.route.uri,self.commercial_mode,self.headsign or '',self.points)
        if pattern_signature in self.timetable.signature_to_jp:
            self.journey_pattern = self.timetable.signature_to_jp[pattern_signature]
        else:
            jp = JourneyPattern(self.timetable,str(len(self.timetable.signature_to_jp)),self.route.uri,self.points,self.headsign,self.commercial_mode)
            self.timetable.signature_to_jp[pattern_signature] = jp
            self.timetable.journey_patterns[jp.uri] = jp
            self.journey_pattern = jp
        del(self.points)

        if self.timedemandgroup in self.timetable.signature_to_timedemandgroup:
            self.timedemandgroup = self.timetable.signature_to_timedemandgroup[self.timedemandgroup]
        else:
            timegroup = TimeDemandGroup(self.timetable,str(len(self.timetable.timedemandgroups)),self.timedemandgroup)
            self.timetable.signature_to_timedemandgroup[self.timedemandgroup] = timegroup
            self.timetable.timedemandgroups[timegroup.uri] = timegroup
            self.timedemandgroup = timegroup

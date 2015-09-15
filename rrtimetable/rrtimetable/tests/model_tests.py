import unittest
from model.transit import *
import datetime

class TestSequenceFunctions(unittest.TestCase):

    def test_equal(self):
        tdata = Timetable(datetime.date(2014,1,1),'Europe/Amsterdam')
        sa = StopArea(tdata,'SA1','Europe/Amsterdam',name='SA1')
        sa = StopArea(tdata,'SA2','Europe/Amsterdam',name='SA2')
        sa = StopArea(tdata,'SA3','Europe/Amsterdam',name='SA3')
        sp = StopPoint(tdata,'SP1','SA1',name='SP1')
        sp = StopPoint(tdata,'SP2','SA2',name='SP1')
        sp = StopPoint(tdata,'SP3','SA3',name='SP1')

        jpp1 = JourneyPatternPoint(tdata,'SP1',forboarding=True,foralighting=True,timingpoint=False)
        jpp2 = JourneyPatternPoint(tdata,'SP1',forboarding=True,foralighting=True,timingpoint=False)
        self.assertEquals(jpp1,jpp2)

    def test_primary_key_constraints(self):
        tdata = Timetable(datetime.date(2014,1,1),'Europe/Amsterdam')
        pm = PhysicalMode(tdata,'BUS',name='Bus')
        cm = CommercialMode(tdata,'BUS',name='Bus')
        sa = StopArea(tdata,'SA1','Europe/Amsterdam')
        sa.name = 'SA1'
        self.assertRaises(ValueError, StopArea,tdata,'SA1','Europe/Amsterdam')
        self.assertEquals(tdata.stop_areas['SA1'].name,'SA1')

        sp = StopPoint(tdata,'SP1','SA1')
        sp.name = 'SP1'
        self.assertRaises(ValueError, StopPoint,tdata,'SP1','SA1')
        self.assertEquals(tdata.stop_points['SP1'].name,'SP1')
 
        op = Operator(tdata,'OP1','Europe/Amsterdam')
        op.name = 'OP1'
        self.assertRaises(ValueError, Operator,tdata,'OP1','Europe/Amsterdam')
        self.assertEquals(tdata.operators['OP1'].name,'OP1')

        l = Line(tdata,'L1','OP1','BUS')
        l.name = 'L1'
        self.assertRaises(ValueError, Line,tdata,'L1','OP1','BUS')
        self.assertEquals(tdata.lines['L1'].name,'L1')

        r = Route(tdata,'R1','L1')
        r.name = 'R1'
        self.assertRaises(ValueError, Route,tdata,'R1','L1')
        self.assertEquals(tdata.routes['R1'].name,'R1')

        vj = VehicleJourney(tdata,'VJ1','R1','BUS')
        self.assertRaises(ValueError, VehicleJourney,tdata,'VJ1','R1','BUS')
        self.assertEquals(tdata.vehicle_journeys['VJ1'].route.line.operator.name,'OP1')

    def test_foreign_key_constraints(self):
        tdata = Timetable(datetime.date(2014,1,1),'Europe/Amsterdam')
        pm = PhysicalMode(tdata,'BUS',name='Bus')
        cm = CommercialMode(tdata,'BUS',name='Bus')
        sa = StopArea(tdata,'SA1','Europe/Amsterdam')
        sa.name = 'SA1'
        sp = StopPoint(tdata,'SP1','SA1')
        self.assertRaises(ValueError, StopPoint,tdata,'SP1','SA2')
        self.assertEquals(tdata.stop_points['SP1'].stop_area.name,'SA1')
        self.assertEquals(sp.stop_area,sa)
        op = Operator(tdata,'OP1','Europe/Amsterdam')
        op.name = 'OP1'

        l = Line(tdata,'L1','OP1','BUS')
        self.assertRaises(ValueError, Line,tdata,'L3','OP2','BUS')
        self.assertEquals(tdata.lines['L1'].operator.name,'OP1')

        r = Route(tdata,'R1','L1')
        self.assertRaises(ValueError, Route,tdata,'R1','L1')
        self.assertEquals(tdata.routes['R1'].line.operator.name,'OP1')

        vj = VehicleJourney(tdata,'VJ1','R1','BUS')
        self.assertRaises(ValueError, Route,tdata,'R1','L1')
        self.assertEquals(tdata.vehicle_journeys['VJ1'].route.line.operator.name,'OP1')

    def test_integration(self):
        tdata = Timetable(datetime.date(2014,1,1),'Europe/Amsterdam')
        pm = PhysicalMode(tdata,'BUS',name='Bus')
        cm = CommercialMode(tdata,'BUS',name='Bus')
        sa = StopArea(tdata,'SA1','Europe/Amsterdam',name='SA1')
        sa = StopArea(tdata,'SA2','Europe/Amsterdam',name='SA2')
        sa = StopArea(tdata,'SA3','Europe/Amsterdam',name='SA3')
        sp = StopPoint(tdata,'SP1','SA1',name='SP1')
        sp = StopPoint(tdata,'SP2','SA2',name='SP1')
        sp = StopPoint(tdata,'SP3','SA3',name='SP1')
        conn = Connection(tdata,'SP1','SP2',120,type=2)
        conn = Connection(tdata,'SP2','SP1',120,type=2)
        op = Operator(tdata,'OP1','Europe/Amsterdam',name='Operator',url='http://www.example.com')
        l = Line(tdata,'L1','OP1','BUS',name='Testline',code='T')
        r = Route(tdata,'R1','L1',direction=1)
        vj = VehicleJourney(tdata,'VJ1','R1','BUS')
        self.assertRaises(ValueError, vj.add_stop,'SP1',-10,-10) #Expect exception on negative times
        vj.add_stop('SP1',900,900,forboarding=True,foralighting=True,timingpoint=True)
        self.assertRaises(ValueError, vj.add_stop,'SP2',600,600) #Expect exception on timetravel
        self.assertRaises(ValueError, vj.add_stop,'SP2',1800,1600) #Expect exception on negative dwell
        vj.add_stop('SP2',1800,1860,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2400,2400,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        vj = VehicleJourney(tdata,'VJ2','R1','BUS')
        vj.setIsValidOn(datetime.date(2014,1,1))
        vj.setIsValidOn(datetime.date(2014,1,2))
        vj.setIsValidOn(datetime.date(2014,1,3))
        self.assertEquals(set((0,1,2)),vj.validity_pattern)
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,1960,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2500,2500,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()
        self.assertEquals(False,vj.journey_pattern.points[-1].forboarding)
        self.assertEquals(False,vj.journey_pattern.points[0].foralighting)
        self.assertEquals(1,len(tdata.journey_patterns))
        self.assertEquals(1,len(tdata.timedemandgroups))
        self.assertRaises(AttributeError,vj.add_stop,'SP3',2400,2400) #Expect that we can no longer modify journeys after finishing

        vj = VehicleJourney(tdata,'VJ3','R1','BUS')
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,2000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2500,2500,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()
        self.assertEquals(1,len(tdata.journey_patterns))
        self.assertEquals(2,len(tdata.timedemandgroups))

        vj = VehicleJourney(tdata,'VJ4','R1','BUS')
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,2000,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()
        self.assertEquals(2,len(tdata.journey_patterns))
        self.assertEquals(3,len(tdata.timedemandgroups))

    def test_utc_dst_off_to_on(self):
        tdata = Timetable(datetime.date(2015,3,28),'Europe/Amsterdam')
        pm = PhysicalMode(tdata,'BUS',name='Bus')
        cm = CommercialMode(tdata,'BUS',name='Bus')
        sa = StopArea(tdata,'SA1','Europe/Amsterdam',name='SA1')
        sa = StopArea(tdata,'SA2','Europe/Amsterdam',name='SA2')
        sa = StopArea(tdata,'SA3','Europe/Amsterdam',name='SA3')
        sp = StopPoint(tdata,'SP1','SA1',name='SP1')
        sp = StopPoint(tdata,'SP2','SA2',name='SP1')
        sp = StopPoint(tdata,'SP3','SA3',name='SP1')
        conn = Connection(tdata,'SP1','SP2',120,type=2)
        conn = Connection(tdata,'SP2','SP1',120,type=2)
        op = Operator(tdata,'OP1','Europe/Amsterdam',name='Operator',url='http://www.example.com')
        l = Line(tdata,'L1','OP1','BUS',name='Testline',code='T')
        r = Route(tdata,'R1','L1',direction=1)
        vj = VehicleJourney(tdata,'VJ1','R1','BUS')
        vj.setIsValidOn(datetime.date(2015,3,28))
        vj.setIsValidOn(datetime.date(2015,3,29))
        vj.add_stop('SP1',900,900,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1800,1860,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2400,2400,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        #Check split on utc offset
        self.assertEquals(1,len(tdata.vehicle_journeys))
        self.assertEquals(2,len(tdata.vehicle_journeys_utc))
        self.assertEquals(3600,tdata.vehicle_journeys_utc[('VJ1',3600)].utc_offset)
        self.assertEquals(7200,tdata.vehicle_journeys_utc[('VJ1',7200)].utc_offset)

        #Correct departuretimes
        self.assertEquals(900,tdata.vehicle_journeys['VJ1'].departure_time)
        self.assertEquals(-2700,tdata.vehicle_journeys_utc[('VJ1',3600)].departure_time)
        self.assertEquals(-6300,tdata.vehicle_journeys_utc[('VJ1',7200)].departure_time)

        #Correct validity_patterns
        self.assertEquals(set([0,1]),tdata.vehicle_journeys['VJ1'].validity_pattern)
        self.assertEquals(set([0]),tdata.vehicle_journeys_utc[('VJ1',3600)].validity_pattern)
        self.assertEquals(set([1]),tdata.vehicle_journeys_utc[('VJ1',7200)].validity_pattern)

    def test_utc_dst_on_to_off(self):
        tdata = Timetable(datetime.date(2015,10,24),'Europe/Amsterdam')
        pm = PhysicalMode(tdata,'BUS',name='Bus')
        cm = CommercialMode(tdata,'BUS',name='Bus')
        sa = StopArea(tdata,'SA1','Europe/Amsterdam',name='SA1')
        sa = StopArea(tdata,'SA2','Europe/Amsterdam',name='SA2')
        sa = StopArea(tdata,'SA3','Europe/Amsterdam',name='SA3')
        sp = StopPoint(tdata,'SP1','SA1',name='SP1')
        sp = StopPoint(tdata,'SP2','SA2',name='SP1')
        sp = StopPoint(tdata,'SP3','SA3',name='SP1')
        conn = Connection(tdata,'SP1','SP2',120,type=2)
        conn = Connection(tdata,'SP2','SP1',120,type=2)
        op = Operator(tdata,'OP1','Europe/Amsterdam',name='Operator',url='http://www.example.com')
        l = Line(tdata,'L1','OP1','BUS',name='Testline',code='T')
        r = Route(tdata,'R1','L1',direction=1)
        vj = VehicleJourney(tdata,'VJ1','R1','BUS')
        vj.setIsValidOn(datetime.date(2015,10,24))
        vj.setIsValidOn(datetime.date(2015,10,25))
        vj.add_stop('SP1',900,900,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1800,1860,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2400,2400,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        #Check split on utc offset
        self.assertEquals(1,len(tdata.vehicle_journeys))
        self.assertEquals(2,len(tdata.vehicle_journeys_utc))
        self.assertEquals(3600,tdata.vehicle_journeys_utc[('VJ1',3600)].utc_offset)
        self.assertEquals(7200,tdata.vehicle_journeys_utc[('VJ1',7200)].utc_offset)

        #Correct departuretimes
        self.assertEquals(900,tdata.vehicle_journeys['VJ1'].departure_time)
        self.assertEquals(-2700,tdata.vehicle_journeys_utc[('VJ1',3600)].departure_time)
        self.assertEquals(-6300,tdata.vehicle_journeys_utc[('VJ1',7200)].departure_time)

        #Correct validity_patterns
        self.assertEquals(set([0,1]),tdata.vehicle_journeys['VJ1'].validity_pattern)
        self.assertEquals(set([0]),tdata.vehicle_journeys_utc[('VJ1',7200)].validity_pattern)
        self.assertEquals(set([1]),tdata.vehicle_journeys_utc[('VJ1',3600)].validity_pattern)

if __name__ == '__main__':
    unittest.main()

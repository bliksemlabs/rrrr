import unittest
import helper
from model.transit import *
import datetime

class TestSequenceFunctions(unittest.TestCase):

    def test_equal(self):
        tdata = Timetable(datetime.date(2014,1,1))
        sa = StopArea(tdata,'SA1',name='SA1')
        sa = StopArea(tdata,'SA2',name='SA2')
        sa = StopArea(tdata,'SA3',name='SA3')
        sp = StopPoint(tdata,'SP1','SA1',name='SP1')
        sp = StopPoint(tdata,'SP2','SA2',name='SP1')
        sp = StopPoint(tdata,'SP3','SA3',name='SP1')
        
        jpp1 = JourneyPatternPoint(tdata,'SP1',forboarding=True,foralighting=True,timingpoint=False)
        jpp2 = JourneyPatternPoint(tdata,'SP1',forboarding=True,foralighting=True,timingpoint=False)
        self.assertEquals(jpp1,jpp2)

    def test_primary_key_constraints(self):
        tdata = Timetable(datetime.date(2014,1,1))
        sa = StopArea(tdata,'SA1')
        sa.name = 'SA1'
        self.assertRaises(ValueError, StopArea,tdata,'SA1')
        self.assertEquals(tdata.stop_areas['SA1'].name,'SA1')

        sp = StopPoint(tdata,'SP1','SA1')
        sp.name = 'SP1'
        self.assertRaises(ValueError, StopPoint,tdata,'SP1','SA1')
        self.assertEquals(tdata.stop_points['SP1'].name,'SP1')
 
        op = Operator(tdata,'OP1')
        op.name = 'OP1'
        self.assertRaises(ValueError, Operator,tdata,'OP1')
        self.assertEquals(tdata.operators['OP1'].name,'OP1')

        l = Line(tdata,'L1','OP1')
        l.name = 'L1'
        self.assertRaises(ValueError, Line,tdata,'L1','OP1')
        self.assertEquals(tdata.lines['L1'].name,'L1')

        r = Route(tdata,'R1','L1')
        r.name = 'R1'
        self.assertRaises(ValueError, Route,tdata,'R1','L1')
        self.assertEquals(tdata.routes['R1'].name,'R1')

        vj = VehicleJourney(tdata,'VJ1','R1')
        self.assertRaises(ValueError, VehicleJourney,tdata,'VJ1','R1')
        self.assertEquals(tdata.vehicle_journeys['VJ1'].route.line.operator.name,'OP1')

    def test_foreign_key_constraints(self):
        tdata = Timetable(datetime.date(2014,1,1))
        sa = StopArea(tdata,'SA1')
        sa.name = 'SA1'
        sp = StopPoint(tdata,'SP1','SA1')
        self.assertRaises(ValueError, StopPoint,tdata,'SP1','SA2')
        self.assertEquals(tdata.stop_points['SP1'].stop_area.name,'SA1')
        self.assertEquals(sp.stop_area,sa)
        op = Operator(tdata,'OP1')
        op.name = 'OP1'

        l = Line(tdata,'L1','OP1')
        self.assertRaises(ValueError, Line,tdata,'L3','OP2')
        self.assertEquals(tdata.lines['L1'].operator.name,'OP1')

        r = Route(tdata,'R1','L1')
        self.assertRaises(ValueError, Route,tdata,'R1','L1')
        self.assertEquals(tdata.routes['R1'].line.operator.name,'OP1')

        vj = VehicleJourney(tdata,'VJ1','R1')
        self.assertRaises(ValueError, Route,tdata,'R1','L1')
        self.assertEquals(tdata.vehicle_journeys['VJ1'].route.line.operator.name,'OP1')

    def test_integration(self):
        tdata = Timetable(datetime.date(2014,1,1))
        sa = StopArea(tdata,'SA1',name='SA1')
        sa = StopArea(tdata,'SA2',name='SA2')
        sa = StopArea(tdata,'SA3',name='SA3')
        sp = StopPoint(tdata,'SP1','SA1',name='SP1')
        sp = StopPoint(tdata,'SP2','SA2',name='SP1')
        sp = StopPoint(tdata,'SP3','SA3',name='SP1')
        conn = Connection(tdata,'SP1','SP2',120,type=2)
        conn = Connection(tdata,'SP2','SP1',120,type=2)
        op = Operator(tdata,'OP1',name='Operator',url='http://www.example.com')
        l = Line(tdata,'L1','OP1',name='Testline',code='T')
        r = Route(tdata,'R1','L1',direction=1)
        vj = VehicleJourney(tdata,'VJ1','R1')
        self.assertRaises(ValueError, vj.add_stop,'SP1',-10,-10) #Expect exception on negative times
        vj.add_stop('SP1',900,900,forboarding=True,foralighting=True,timingpoint=True)
        self.assertRaises(ValueError, vj.add_stop,'SP2',600,600) #Expect exception on timetravel
        self.assertRaises(ValueError, vj.add_stop,'SP2',1800,1600) #Expect exception on negative dwell
        vj.add_stop('SP2',1800,1860,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2400,2400,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        vj = VehicleJourney(tdata,'VJ2','R1')
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

        vj = VehicleJourney(tdata,'VJ3','R1')
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,2000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2500,2500,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()
        self.assertEquals(1,len(tdata.journey_patterns))
        self.assertEquals(2,len(tdata.timedemandgroups))

        vj = VehicleJourney(tdata,'VJ4','R1')
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,2000,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()
        self.assertEquals(2,len(tdata.journey_patterns))
        self.assertEquals(3,len(tdata.timedemandgroups))
if __name__ == '__main__':
    unittest.main()

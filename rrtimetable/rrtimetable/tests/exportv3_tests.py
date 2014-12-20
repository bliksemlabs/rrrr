import unittest
import helper
from model.transit import *
from exporter.timetable3 import export
import datetime

class TestSequenceFunctions(unittest.TestCase):

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
        vj.add_stop('SP1',900,900,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1800,1860,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2400,2400,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        vj = VehicleJourney(tdata,'VJ2','R1')
        vj.setIsValidOn(datetime.date(2014,1,1))
        vj.setIsValidOn(datetime.date(2014,1,2))
        vj.setIsValidOn(datetime.date(2014,1,3))
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,1960,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2500,2500,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        vj = VehicleJourney(tdata,'VJ3','R1')
        vj.setIsValidOn(datetime.date(2014,1,1))
        vj.setIsValidOn(datetime.date(2014,1,2))
        vj.setIsValidOn(datetime.date(2014,1,3))
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,2000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP3',2500,2500,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()

        vj = VehicleJourney(tdata,'VJ4','R1')
        vj.setIsValidOn(datetime.date(2014,1,1))
        vj.setIsValidOn(datetime.date(2014,1,2))
        vj.setIsValidOn(datetime.date(2014,1,3))
        vj.add_stop('SP1',1000,1000,forboarding=True,foralighting=True,timingpoint=True)
        vj.add_stop('SP2',1900,2000,forboarding=True,foralighting=True,timingpoint=True)
        vj.finish()
        export(tdata)

if __name__ == '__main__':
    unittest.main()

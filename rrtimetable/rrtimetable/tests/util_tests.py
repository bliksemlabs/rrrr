import unittest
import helper
from model.utils import *
import datetime

class TestSequenceFunctions(unittest.TestCase):

    def test_utc(self):
        utc_offset,utc_deptime = utc_time_gtfs(datetime.date(2015,2,23),3600,'Europe/Amsterdam')
        self.assertEquals(utc_offset,3600)
        self.assertEquals(utc_deptime,0)

        utc_offset,utc_deptime = utc_time_gtfs(datetime.date(2015,3,28),3600,'Europe/Amsterdam')
        self.assertEquals(utc_offset,3600)
        self.assertEquals(utc_deptime,0)

        utc_offset,utc_deptime = utc_time_gtfs(datetime.date(2015,3,29),3600,'Europe/Amsterdam')
        self.assertEquals(utc_offset,7200)
        self.assertEquals(utc_deptime,-3600)

        utc_offset,utc_deptime = utc_time_gtfs(datetime.date(2015,3,29),10*60*60,'Europe/Amsterdam')
        self.assertEquals(utc_offset,7200)
        self.assertEquals(utc_deptime,8*60*60)

if __name__ == '__main__':
    unittest.main()

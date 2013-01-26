#!/usr/bin/python

from imposm.parser import OSMParser

class OSMProcessor :
    nways = 0
    ref_count = {}
    nrefs = 0
    def count_refs(self, ways):
        for osmid, tags, refs in ways:
            self.nways += 1
            if self.nways % 100000 == 0 : 
                print '%d ways' % self.nways
            for node_id in refs:
                self.nrefs += 1
#            if 'highway' in tags:
#                for node_id in refs :
#                    if node_id in self.ref_count :
#                        self.ref_count[node_id] += 1
#                    else :
#                        self.ref_count[node_id] = 1

processor = OSMProcessor()
# instantiate counter and parser and start parsing

parser = OSMParser(concurrency=1, ways_callback=processor.count_refs)

#parser.parse('/var/otp/graphs/idf/ile-de-france.osm.pbf')
parser.parse('/var/otp/graphs/benelux/planet-benelux.osm.pbf')

print 'done. total node refs:', processor.nrefs
#for node, count in processor.node_count.iteritems() :
#    if count > 1 :
#        print node, count


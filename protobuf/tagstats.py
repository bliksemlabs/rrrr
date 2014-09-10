#!/usr/bin/python

# http://imposm.org/docs/imposm/latest/install.html
# sudo apt-get install build-essential python-dev protobuf-compiler libprotobuf-dev libtokyocabinet-dev python-psycopg2 libgeos-c1 python-pip
# sudo pip install imposm.parser
    
from imposm.parser import OSMParser
import sys

skip_keys = ['name', 'note', 'operator', 'source', 'tiger:', 'nhd', 'zip', 'RLIS:', 'gnis', 'addr:', 'import', 'created', 'CCGIS', 'website']

retain_keys = ['building', 'highway', 'footway', 'cycleway', 'surface', 'railway', 'amenity', 'public_transport', 'bridge', 'embankment', 'tunnel', 'bicycle', 'oneway', 'natural', 'lanes', 'landuse', "RLIS:bicycle", "CCGIS:bicycle"]

def dump_tags(tags):
    weighted = []
    for (key, value), count in tags.iteritems():
        weight = (len(key) + len(value) + 2) * count
        weighted.append((key, value, count, weight))
    weighted.sort(key=lambda x: x[3], reverse=True)
    for num, tag in enumerate(weighted):
        print num, "%s=%s (%d)" % tag[:3]

class TagCounter(object):
    way_tags = {}
    node_tags = {}
    key_weights = {}
    n_nodes = 0
    n_ways = 0

    def count_tags(self, target, tags):
        for tag in tags.iteritems():
            key, value = tag
            #if any(key.startswith(sk) for sk in skip_keys):
            if key not in retain_keys:
                continue
            if key not in self.key_weights:
                self.key_weights[key] = 0
            self.key_weights[key] += (len(key) + len(value) + 2)
            if tag not in target:
                target[tag] = 1
            target[tag] += 1

    def ways(self, ways):
        for osmid, tags, refs in ways:
            self.n_ways += 1
            if (self.n_ways % 10000 == 0):
                print "%d ways" % self.n_ways
            self.count_tags(self.way_tags, tags)            

    def nodes(self, nodes):
        for osmid, tags, coord in nodes:
            self.n_nodes += 1
            if (self.n_nodes % 10000 == 0):
                print "%d nodes" % self.n_nodes
            self.count_tags(self.node_tags, tags)            

tc = TagCounter()
p = OSMParser(concurrency=4, ways_callback=tc.ways, nodes_callback=tc.nodes)
p.parse(sys.argv[1])

print "KEY WEIGHTS"
worst = [x for x in tc.key_weights.iteritems()]
worst.sort(key = lambda x : x[1])
for key, weight in worst[-100:]:
    print key, weight
print "NODES"
dump_tags(tc.node_tags)
print "WAYS"
dump_tags(tc.way_tags)
print "AVOID"
print skip_keys



#!/usr/bin/python

from graphserver.ext.gtfs.gtfsdb import GTFSDatabase
from struct import Struct

gdb = GTFSDatabase('/home/abyrd/trimet.gtfsdb')

out = open("/home/abyrd/timetable.dat", "wb")

intpacker = Struct('i')
def writeint(x) :
    out.write(intpacker.pack(x));

idx_for_stopid = {}
coords = []
idx = 0
for (sid, name, lat, lon) in gdb.stops() :
    idx_for_stopid[sid] = idx
    coords.append((lat,lon))
    idx += 1

nstops = idx    
assert nstops == len(coords)
writeint(nstops)
nroutes = 2
writeint(nroutes)
packer = Struct('ff')
for (lat, lon) in coords:
    out.write(packer.pack(lat, lon));
            
for i in range(nroutes) :
    writeint(i);
for i in range(nroutes) :
    writeint(i);

out.close();


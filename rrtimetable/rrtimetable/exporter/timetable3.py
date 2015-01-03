import helper
from utils import *
import operator
import sys

NUMBER_OF_DAYS = 32

class Index():
    def __init__(self):
        self.operators = []
        self.idx_for_operator_uri = {}
        self.lines = []
        self.idx_for_line_uri = {}
        self.routes = []
        self.idx_for_route_uri = {}
        self.journey_patterns = []
        self.idx_for_journey_pattern_uri = {}
        self.stop_points = []
        self.idx_for_stop_point_uri = {}
        self.stop_areas = []
        self.idx_for_stop_area_uri = {}
        self.validity_pattern_for_journey_pattern_uri = {}
        self.timedemandgroups = []
        self.idx_for_timedemandgroup_uri = {}
        self.journey_patterns_at_stop_point = {}
        self.vehicle_journeys_in_journey_pattern = {}
        self.connections_from_stop_point = {}
        self.connections_point_to_point = {}

        self.loc_for_headsign = {}
        self.headsigns = []
        self.headsign_length = 0

        self.idx_for_productcategory = {}
        self.productcategories = []

        self.idx_for_linecode = {}
        self.linecodes = []

    def put_headsign(self,headsign):
        if headsign in self.loc_for_headsign:
            return self.loc_for_headsign[headsign]
        self.loc_for_headsign[headsign] = self.headsign_length
        self.headsign_length += (len(headsign) + 1)
        self.headsigns.append(headsign)
        return self.loc_for_headsign[headsign]

    def put_productcategory(self,productcategory):
        if productcategory in self.idx_for_productcategory:
            return self.idx_for_productcategory[productcategory]
        self.idx_for_productcategory[productcategory] = len(self.idx_for_productcategory)
        self.productcategories.append(productcategory)
        return self.idx_for_productcategory[productcategory]

    def put_linecode(self,linecode):
        if linecode in self.idx_for_linecode:
            return self.idx_for_linecode[linecode]
        self.idx_for_linecode[linecode] = len(self.idx_for_linecode)
        self.linecodes.append(linecode)
        return self.idx_for_linecode[linecode]

def make_idx(tdata):
    index = Index()
    for vj in sorted(tdata.vehicle_journeys.values(), key= lambda vj: (vj.route.line.operator.uri,vj.route.line.uri,vj.route.uri,vj.departure_time)):
        if len(vj.validity_pattern) == 0 or min(vj.validity_pattern) >= NUMBER_OF_DAYS:
            continue
        if vj.journey_pattern.uri not in index.validity_pattern_for_journey_pattern_uri:
            index.validity_pattern_for_journey_pattern_uri[vj.journey_pattern.uri] = set([])
        index.validity_pattern_for_journey_pattern_uri[vj.journey_pattern.uri].update(vj.validity_pattern)

        if vj.journey_pattern.route.line.operator.uri not in index.idx_for_operator_uri:
            index.idx_for_operator_uri[vj.journey_pattern.route.line.operator.uri] = len(index.idx_for_operator_uri)
            index.operators.append(vj.journey_pattern.route.line.operator)

        if vj.journey_pattern.route.line.uri not in index.idx_for_line_uri:
            index.idx_for_line_uri[vj.journey_pattern.route.line.uri] = len(index.idx_for_line_uri)
            index.lines.append(vj.journey_pattern.route.line)

        if vj.journey_pattern.route.uri not in index.idx_for_route_uri:
            index.idx_for_route_uri[vj.journey_pattern.route.uri] = len(index.idx_for_route_uri)
            index.routes.append(vj.journey_pattern.route)

        if vj.journey_pattern.uri not in index.idx_for_journey_pattern_uri:
            index.idx_for_journey_pattern_uri[vj.journey_pattern.uri] = len(index.idx_for_journey_pattern_uri)
            index.journey_patterns.append(vj.journey_pattern)

        if vj.journey_pattern.uri not in index.vehicle_journeys_in_journey_pattern:
            index.vehicle_journeys_in_journey_pattern[vj.journey_pattern.uri] = []
        index.vehicle_journeys_in_journey_pattern[vj.journey_pattern.uri].append(vj)

        if vj.timedemandgroup.uri not in index.idx_for_timedemandgroup_uri:
            index.idx_for_timedemandgroup_uri[vj.timedemandgroup.uri] = len(index.idx_for_timedemandgroup_uri)
            index.timedemandgroups.append(vj.timedemandgroup)
        for jpp in vj.journey_pattern.points:
            if jpp.stop_point.uri not in index.idx_for_stop_point_uri:
                index.idx_for_stop_point_uri[jpp.stop_point.uri] = len(index.stop_points)
                index.stop_points.append(jpp.stop_point)
            if jpp.stop_point.uri not in index.journey_patterns_at_stop_point:
                index.journey_patterns_at_stop_point[jpp.stop_point.uri] = set([])
            index.journey_patterns_at_stop_point[jpp.stop_point.uri].add(vj.journey_pattern.uri)
            if jpp.stop_point.stop_area.uri not in index.idx_for_stop_area_uri:
                index.idx_for_stop_point_uri[jpp.stop_point.stop_area.uri] = len(index.stop_areas)
                index.stop_areas.append(jpp.stop_point.stop_area)

    for conn in tdata.connections.values():
        if conn.from_stop_point.uri not in index.idx_for_stop_point_uri or conn.to_stop_point.uri not in index.idx_for_stop_point_uri:
            continue #connection to or from unknown stop_point
        if conn.from_stop_point.uri not in index.connections_from_stop_point:
            index.connections_from_stop_point[conn.from_stop_point.uri] = []
        index.connections_from_stop_point[conn.from_stop_point.uri].append(conn)
    if len(index.journey_patterns) == 0:
        print "No valid journey_patterns found to export to this timetable. Exiting..."
        sys.exit(1)
    print '--------------------------'
    return index

def write_stop_point_idx(out,index,stop_uri):
    if len(index.stop_points) <= 65535:
        writeshort(out,index.idx_for_stop_point_uri[stop_uri])
    else:
        writeint(out,index.idx_for_stop_point_uri[stop_uri])

def export_sp_coords(tdata,index,out):
    index.loc_stop_coords = out.tell()
    for sp in index.stop_points:
        write2floats(out,sp.latitude or 0.0, sp.longitude or 0.0)

def export_journey_pattern_point_stop(tdata,index,out):
    write_text_comment(out,"JOURNEY_PATTERN_POINT STOP")
    index.loc_journey_pattern_points = tell(out)
    index.offset_jpp = []
    offset = 0
    index.n_jpp = 0
    for jp in index.journey_patterns:
        index.offset_jpp.append(offset)
        for jpp in jp.points:
            index.n_jpp += 1
            write_stop_point_idx(out,index,jpp.stop_point.uri)
            offset += 1

def export_journey_pattern_point_attributes(tdata,index,out):
    write_text_comment(out,"STOPS ATTRIBUTES BY JOURNEY_PATTERN")
    index.loc_journey_pattern_point_attributes = tell(out)
    index.offset_jpp_attributes = []
    offset = 0
    for jp in index.journey_patterns:
        index.offset_jpp_attributes.append(offset)
        for jpp in jp.points:
            attr = 0
            if jpp.timingpoint:
                attr |= 1
            if jpp.forboarding:
                attr |= 2
            if jpp.foralighting:
                attr |= 4
            writebyte(out,attr)
            offset += 1

timedemandgroup_t = Struct('HH')
def export_timedemandgroups(tdata,index,out):
    write_text_comment(out,"TIMEDEMANDGROUPS")
    index.loc_timedemandgroups = tell(out)
    index.offset_for_timedemandgroup_uri = {}
    tp_offset = 0
    for tp in index.timedemandgroups:
        index.offset_for_timedemandgroup_uri[tp.uri] = tp_offset
        for tpp in tp.points:
            out.write(timedemandgroup_t.pack(tpp.drivetime >> 2, tpp.totaldrivetime >> 2))
            tp_offset += 1
    index.n_tpp = tp_offset

def export_vj_in_jp(tdata,index,out):
    write_text_comment(out,"VEHICLE JOURNEYS IN JOURNEY_PATTERN")
    index.loc_vehicle_journeys = tell(out)
    tioffset = 0
    index.vj_ids_offsets = []
    vj_t = Struct('IHH')
    for jp in index.journey_patterns:
        index.vj_ids_offsets.append(tioffset)
        for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
            vj_attr = 0
            out.write(vj_t.pack(index.offset_for_timedemandgroup_uri[vj.timedemandgroup.uri], vj.departure_time >> 2, vj_attr))
            tioffset += 1

def export_jpp_at_sp(tdata,index,out):
    write_text_comment(out,"JOURNEY_PATTERNS AT STOP")
    index.loc_jp_at_sp = tell(out)
    index.jpp_at_sp_offsets = []
    n_offset = 0
    for sp in index.stop_points:
        jp_uris = index.journey_patterns_at_stop_point[sp.uri]
        index.jpp_at_sp_offsets.append(n_offset)
        for jp_uri in jp_uris:
            writeint(out,index.idx_for_journey_pattern_uri[jp_uri])
            n_offset += 1
    index.jpp_at_sp_offsets.append(n_offset) #sentinel
    index.n_jpp_at_sp = n_offset

def export_transfers(tdata,index,out):
    print "saving transfer stops (footpaths)"
    write_text_comment(out,"TRANSFER TARGET STOPS")
    index.loc_transfer_target_stop_points = tell(out)

    index.transfers_offsets = []
    offset = 0
    transfertimes = []
    for sp in index.stop_points:
        index.transfers_offsets.append(offset)
        if sp.uri not in index.connections_from_stop_point:
            continue
        for conn in index.connections_from_stop_point[sp.uri]:
            if (int(conn.min_transfer_time) >> 2) > 255:
                continue
            write_stop_point_idx(out,index,conn.to_stop_point.uri)
            transfertimes.append(conn.min_transfer_time)
            offset += 1
    assert len(transfertimes) == offset
    index.transfers_offsets.append(offset) #sentinel
    index.n_connections = offset

    print "saving transfer times (footpaths)"
    write_text_comment(out,"TRANSFER TIMES")
    index.loc_transfer_dist_meters = tell(out)

    for transfer_time in transfertimes:
        writebyte(out,(int(transfer_time) >> 2))

def export_stop_indices(tdata,index,out):
    print "saving stop indexes"
    write_text_comment(out,"STOP STRUCTS")
    index.loc_stops = tell(out)
    struct_2i = Struct('II')
    print len(index.jpp_at_sp_offsets),len(index.transfers_offsets)
    assert len(index.jpp_at_sp_offsets) == len(index.transfers_offsets)
    for stop in zip (index.jpp_at_sp_offsets, index.transfers_offsets) :
        out.write(struct_2i.pack(*stop));

def export_stop_point_attributes(tdata,index,out):
    print "saving stop attributes"
    write_text_comment(out,"STOP Attributes")
    index.loc_stop_point_attributes = tell(out)
    for sp in index.stop_points:
        attr = 0
        writebyte(out,attr)

def export_jp_structs(tdata,index,out):
    print "saving route indexes"
    write_text_comment(out,"ROUTE STRUCTS")
    index.loc_journey_patterns = tell(out)
    route_t = Struct('3I8H')
    jpp_offsets = index.offset_jpp
    trip_ids_offsets = index.vj_ids_offsets
    jp_attributes = []

    nroutes = len(index.journey_patterns)

    jp_n_jpp = []
    jp_n_vj = []

    index.idx_for_operator = {}
    index.jp_operators = []
    operator_offsets = []

    linecode_offsets = []
    productcategory_offsets = []
    headsign_offsets=[]
    jp_min_time = []
    jp_max_time = []
    for jp in index.journey_patterns:
        jp_n_jpp.append(len(jp.points))
        jp_n_vj.append(len(index.vehicle_journeys_in_journey_pattern[jp.uri]))
        jp_min_time.append(min([vj.departure_time for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]]) >> 2)
        jp_max_time.append(max([vj.departure_time+vj.timedemandgroup.points[-1].totaldrivetime for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]]) >> 2)

        productcategory_offsets.append(index.put_productcategory(jp.productcategory or '')) 
        headsign_offsets.append(index.put_headsign(jp.headsign or '')) 
        linecode_offsets.append(index.put_linecode(jp.route.line.code or '')) 

        operator = jp.route.line.operator.uri
        if operator not in index.idx_for_operator:
            index.idx_for_operator[operator] = len(index.idx_for_operator)
            index.jp_operators.append(operator)
        operator_offsets.append(index.idx_for_operator[operator]) 
        jp_attributes.append(1 << jp.route.route_type)
    jp_t_fields = [jpp_offsets, trip_ids_offsets,headsign_offsets, jp_n_jpp, jp_n_vj,jp_attributes,operator_offsets,linecode_offsets,productcategory_offsets,jp_min_time, jp_max_time]
    for l in jp_t_fields :
        # the extra last route is a sentinel so we can derive list lengths for the last true route.
        assert len(l) == nroutes
    for route in zip (*jp_t_fields) :
        # print route
        out.write(route_t.pack(*route));
    out.write(route_t.pack(jpp_offsets[-1]+1,0,0,0,0,0,0,0,0,0, 0)) #Sentinel

def validity_mask(days):
    mask = 0
    for day in days:
        if day < NUMBER_OF_DAYS:
            mask |= 1 << day
    return mask

def export_vj_validities(tdata,index,out):
    print "writing bitfields indicating which days each trip is active" 
    # note that bitfields are ordered identically to the trip_ids table, and offsets into that table can be reused
    write_text_comment(out,"VJ ACTIVE BITFIELDS")
    index.loc_vj_active = tell(out)

    for jp in index.journey_patterns:
        for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
            writeint(out,validity_mask(vj.validity_pattern))

def export_jp_validities(tdata,index,out):
    print "writing bitfields indicating which days each trip is active" 
    # note that bitfields are ordered identically to the trip_ids table, and offsets into that table can be reused
    write_text_comment(out,"JP ACTIVE BITFIELDS")
    index.loc_jp_active = tell(out)
    n_zeros = 0
    for jp in index.journey_patterns:
        writeint(out,validity_mask(index.validity_pattern_for_journey_pattern_uri[jp.uri]))

def export_platform_codes(tdata,index,out):
    print "writing out platformcodes for stops"
    write_text_comment(out,"PLATFORM CODES")
    index.loc_platformcodes = write_string_table(out,[sp.platformcode or '' for sp in index.stop_points])

def export_stop_point_names(tdata,index,out):

    nameloc_for_name = {}
    index.sp_nameloc_for_idx = []
    index.sp_namesize = 0
    idx = 0
    for sp in index.stop_points:
        name = sp.name or ''
        if name in nameloc_for_name:
            nameloc = nameloc_for_name[name]
        else:
            nameloc = index.sp_namesize
            nameloc_for_name[name] = nameloc
            index.sp_namesize += 1 + len(name)
        index.sp_nameloc_for_idx.append(nameloc)
        idx += 1
    nstops = idx
    assert len(index.sp_nameloc_for_idx) == idx

    print "writing out stop names to string table"
    write_text_comment(out,"STOP NAME")
    stop_names = sorted(nameloc_for_name.iteritems(), key=operator.itemgetter(1))
    index.loc_stop_names = tell(out)
    for stop_name,nameloc in stop_names:
        assert nameloc == out.tell() - index.loc_stop_names
        out.write(stop_name+'\0')

    print "writing out locations for stopnames"
    write_text_comment(out,"STOP NAME LOCATIONS")
    index.loc_stop_nameidx = tell(out)
    for nameloc in index.sp_nameloc_for_idx:
        writeint(out,nameloc)
    writeint(out,0)

def export_operators(tdata,index,out):
    print "writing out agencies to string table"
    write_text_comment(out,"OPERATOR IDS")
    uris = index.jp_operators
    index.n_operators = len(uris)
    index.loc_operator_ids = write_string_table(out,[index.operators[index.idx_for_operator_uri[uri]].uri for uri in uris])
    write_text_comment(out,"OPERATOR NAMES")
    index.loc_operator_names = write_string_table(out,[index.operators[index.idx_for_operator_uri[uri]].name or '' for uri in uris])
    write_text_comment(out,"OPERATOR URLS")
    index.loc_operator_urls = write_string_table(out,[index.operators[index.idx_for_operator_uri[uri]].url or '' for uri in uris])

def export_headsigns(tdata,index,out):
    print "writing out headsigns to string table"
    write_text_comment(out,"HEADSIGNS")
    sorted_headsigns = sorted(index.loc_for_headsign.iteritems(), key=operator.itemgetter(1))
    index.loc_headsign = tell(out)
    written_length = 0
    for headsign in index.headsigns:
        out.write(headsign+'\0')
        written_length += len(headsign) + 1
    assert written_length == index.headsign_length


def export_linecodes(tdata,index,out):
    write_text_comment(out,"LINE CODES")
    index.loc_line_codes = write_string_table(out,index.linecodes)

def export_productcategories(tdata,index,out):
    write_text_comment(out,"PRODUCT CATEGORIES")
    index.loc_productcategories = write_string_table(out,index.productcategories)

def export_line_uris(tdata,index,out):
    # maybe no need to store route IDs: report trip ids and look them up when reconstructing the response
    print "writing line ids to string table"
    write_text_comment(out,"LINE IDS")
    index.loc_line_uris = write_string_table(out,[jp.route.line.uri for jp in index.journey_patterns])

def export_sp_uris(tdata,index,out):
    print "writing out sorted stop ids to string table"
    # stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
    write_text_comment(out,"STOP IDS")
    index.loc_stop_point_uris = write_string_table(out,[sp.uri for sp in index.stop_points])

def export_vj_uris(tdata,index,out):
     all_vj_ids = [] 
     for jp in index.journey_patterns:
         for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
             all_vj_ids.append(vj.uri)
     index.n_vj = len(all_vj_ids)
     print "writing trip ids to string table" 
     # note that trip_ids are ordered by departure time within trip bundles (routes), which are themselves in arbitrary order. 
     write_text_comment(out,"VJ IDS")
     index.loc_vj_uris = write_string_table(out,all_vj_ids)
     index.n_vj = len(all_vj_ids)

def write_header (out,index) :
    """ Write out a file header containing offsets to the beginning of each subsection.
    Must match struct transit_data_header in transitdata.c """
    out.seek(0)
    htext = "TTABLEV3"


    packed = struct_header.pack(htext,
        index.calendar_start_time,
        index.dst_mask,
        index.n_stops, # n_stops
        index.n_stops, # n_stop_attributes
        index.n_stops, # n_stop_coords
        index.n_jp, # n_routes
        index.n_jpp, # n_route_stops
        index.n_jpp, # n_route_stop_attributes
        index.n_tpp, # n_stop_times
        index.n_vj, # n_vjs
        index.n_jpp_at_sp, # n_stop_routes
        index.n_connections, #n_transfer_target_stop
        index.n_connections, #n_transfer_dist_meters
        index.n_vj, #n_trip_active
        index.n_jp, # n_route_active
        index.n_stops, # n_platformcodes
        index.sp_namesize, # n_stop_names (length of the object)
        len(index.sp_nameloc_for_idx) + 1, # n_stop_nameidx
        index.n_operators, # n_agency_ids
        index.n_operators, # n_agency_names
        index.n_operators, # n_agency_urls
        index.headsign_length, # n_headsigns (length of the object)
        len(index.idx_for_linecode), # n_route_shortnames
        len(index.idx_for_productcategory), # n_productcategories
        len(index.journey_patterns), # n_route_ids
        index.n_stops, # n_stop_ids
        index.n_vj, # n_trip_ids

        index.loc_stops,
        index.loc_stop_point_attributes,
        index.loc_stop_coords,
        index.loc_journey_patterns,
        index.loc_journey_pattern_points,
        index.loc_journey_pattern_point_attributes, 
        index.loc_timedemandgroups,
        index.loc_vehicle_journeys,
        index.loc_jp_at_sp,
        index.loc_transfer_target_stop_points,
        index.loc_transfer_dist_meters,
        index.loc_vj_active,
        index.loc_jp_active,
        index.loc_platformcodes,
        index.loc_stop_names,
        index.loc_stop_nameidx,
        index.loc_operator_ids,
        index.loc_operator_names,
        index.loc_operator_urls,
        index.loc_headsign,
        index.loc_line_codes,
        index.loc_productcategories,
        index.loc_line_uris,
        index.loc_stop_point_uris,
        index.loc_vj_uris,
    )
    print 
    out.write(packed)



struct_header = Struct('8sQ51I')

def export(tdata):
    index = make_idx(tdata)
    index.dst_mask = 0
    index.calendar_start_time = time.mktime((tdata.validfrom).timetuple())
    index.n_stops = len(index.stop_points)
    index.n_jp = len(index.journey_patterns)
    out = open('timetable.dat','wb')
    out.seek(struct_header.size)

    export_sp_coords(tdata,index,out)
    export_journey_pattern_point_stop(tdata,index,out)
    export_journey_pattern_point_attributes(tdata,index,out)
    export_timedemandgroups(tdata,index,out)
    export_vj_in_jp(tdata,index,out)
    export_jpp_at_sp(tdata,index,out)
    export_transfers(tdata,index,out)
    export_stop_indices(tdata,index,out)
    export_stop_point_attributes(tdata,index,out)
    export_jp_structs(tdata,index,out)
    export_vj_validities(tdata,index,out)
    export_jp_validities(tdata,index,out)
    export_platform_codes(tdata,index,out)
    export_stop_point_names(tdata,index,out)
    export_operators(tdata,index,out)
    export_headsigns(tdata,index,out)
    export_linecodes(tdata,index,out)
    export_productcategories(tdata,index,out)
    export_line_uris(tdata,index,out)
    export_sp_uris(tdata,index,out)
    export_vj_uris(tdata,index,out)
    print "reached end of timetable file"
    write_text_comment(out,"END TTABLEV2")
    index.loc_eof = tell(out)
    print "rewinding and writing header... ",
    write_header(out,index)

    out.flush()
    out.close()

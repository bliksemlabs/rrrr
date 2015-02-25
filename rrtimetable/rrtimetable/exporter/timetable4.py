import helper
from utils import *
import operator
import sys
import datetime

# Notes:

# UTC and timezones
#
# We normalize all times to UTC, to properly route through DST-changes and multiple timezones.
# However since we are using unsigned integers, we cannot allow negative times.
# To avoid that we take the highest UTC offset of all vehicle_journey's in the timetable and use that as a global offset.
# All vehicle_journey's have the number of 15-minutes, offset from that global utc offset.
# We picked a 15 minute resolution for these offsets to allow them to store in int8_t

NUMBER_OF_DAYS = 32
MIN_WAITTIME = 120
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
        self.commercial_modes = []
        self.idx_for_commercial_mode_uri = {}
        self.physical_modes = []
        self.idx_for_physical_mode_uri = {}

        self.journey_patterns_at_stop_point = {}
        self.vehicle_journeys_in_journey_pattern = {}
        self.connections_from_stop_point = {}
        self.connections_point_to_point = {}

        self.loc_for_string = {}
        self.strings = []
        self.string_length = 0
        
    def put_string(self,string):
        if string in self.loc_for_string:
            return self.loc_for_string[string]
        self.loc_for_string[string] = self.string_length
        self.string_length += (len(string) + 1)
        self.strings.append(string)
        return self.loc_for_string[string]

def make_idx(tdata):
    index = Index()
    index.global_utc_offset = -999999
    for vj in sorted(tdata.vehicle_journeys_utc.values(), key= lambda vj: (vj.route.line.operator.uri,vj.route.line.uri,vj.route.uri,vj.departure_time)):
        if len(vj.validity_pattern) == 0 or min(vj.validity_pattern) >= NUMBER_OF_DAYS:
            continue
        if vj.utc_offset > index.global_utc_offset:
            index.global_utc_offset = vj.utc_offset
        if vj.journey_pattern.uri not in index.validity_pattern_for_journey_pattern_uri:
            index.validity_pattern_for_journey_pattern_uri[vj.journey_pattern.uri] = set([])
        index.validity_pattern_for_journey_pattern_uri[vj.journey_pattern.uri].update(vj.validity_pattern)

        if vj.journey_pattern.route.line.operator.uri not in index.idx_for_operator_uri:
            index.idx_for_operator_uri[vj.journey_pattern.route.line.operator.uri] = len(index.idx_for_operator_uri)
            index.operators.append(vj.journey_pattern.route.line.operator)

        if vj.journey_pattern.route.line.uri not in index.idx_for_line_uri:
            index.idx_for_line_uri[vj.journey_pattern.route.line.uri] = len(index.idx_for_line_uri)
            index.lines.append(vj.journey_pattern.route.line)
            line = vj.journey_pattern.route.line
            if line.physical_mode.uri not in index.idx_for_physical_mode_uri:
                index.idx_for_physical_mode_uri[line.physical_mode.uri] = len(index.idx_for_physical_mode_uri)
                index.physical_modes.append(line.physical_mode)

        if vj.journey_pattern.route.uri not in index.idx_for_route_uri:
            index.idx_for_route_uri[vj.journey_pattern.route.uri] = len(index.idx_for_route_uri)
            index.routes.append(vj.journey_pattern.route)

        if vj.journey_pattern.uri not in index.idx_for_journey_pattern_uri:
            index.idx_for_journey_pattern_uri[vj.journey_pattern.uri] = len(index.idx_for_journey_pattern_uri)
            index.journey_patterns.append(vj.journey_pattern)

        if vj.journey_pattern.uri not in index.vehicle_journeys_in_journey_pattern:
            index.vehicle_journeys_in_journey_pattern[vj.journey_pattern.uri] = []
        index.vehicle_journeys_in_journey_pattern[vj.journey_pattern.uri].append(vj)

        if vj.journey_pattern.commercial_mode.uri not in index.idx_for_commercial_mode_uri:
            index.idx_for_commercial_mode_uri[vj.journey_pattern.commercial_mode.uri] = len(index.idx_for_commercial_mode_uri)
            index.commercial_modes.append(vj.journey_pattern.commercial_mode)

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
                index.idx_for_stop_area_uri[jpp.stop_point.stop_area.uri] = len(index.stop_areas)
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

def write_stop_point_idx(out,index,stop_point_uri):
    if len(index.stop_points) <= 65535:
        writeshort(out,index.idx_for_stop_point_uri[stop_point_uri])
    else:
        writeint(out,index.idx_for_stop_point_uri[stop_point_uri])

def write_stop_area_idx(out,index,stop_area_uri):
    if len(index.stop_points) <= 65535:
        writeshort(out,index.idx_for_stop_area_uri[stop_area_uri])
    else:
        writeint(out,index.idx_for_stop_area_uri[stop_area_uri])

def write_list_of_strings(out,index,list):
    loc = tell(out)
    for x in list:
        writeint(out,index.put_string(x or ''))
    return loc

def export_sp_coords(tdata,index,out):
    write_text_comment(out,"STOP POINT COORDS")
    index.loc_stop_point_coords = out.tell()
    for sp in index.stop_points:
        write2floats(out,sp.latitude or 0.0, sp.longitude or 0.0)

def export_sa_coords(tdata,index,out):
    write_text_comment(out,"STOP AREA COORDS")
    index.loc_stop_area_coords = out.tell()
    for sa in index.stop_areas:
        write2floats(out,sa.latitude or 0.0, sa.longitude or 0.0)

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

def export_journey_pattern_point_headsigns(tdata,index,out):
    write_text_comment(out,"JOURNEY_PATTERN_POINT HEADSIGN")
    index.loc_journey_pattern_point_headsigns = tell(out)
    index.offset_jpp = []
    offset = 0
    for jp in index.journey_patterns:
        index.offset_jpp.append(offset)
        for jpp in jp.points:
            writeint(out,index.put_string(jpp.headsign or jp.headsign or ''))
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
            out.write(vj_t.pack(index.offset_for_timedemandgroup_uri[vj.timedemandgroup.uri], (vj.departure_time+index.global_utc_offset) >> 2, vj_attr))
            tioffset += 1

def export_jpp_at_sp(tdata,index,out):
    write_text_comment(out,"JOURNEY_PATTERNS AT STOP")
    index.loc_jp_at_sp = tell(out)
    index.jpp_at_sp_offsets = []
    n_offset = 0
    for sp in index.stop_points:
        jp_uris = index.journey_patterns_at_stop_point[sp.uri]
        index.jpp_at_sp_offsets.append(n_offset)
        for jp_uri in set(jp_uris):
            writeshort(out,index.idx_for_journey_pattern_uri[jp_uri])
            n_offset += 1
    index.jpp_at_sp_offsets.append(n_offset) #sentinel
    index.n_jpp_at_sp = n_offset

def export_sa_for_sp(tdata,index,out):
    write_text_comment(out,"STOP_POINT -> STOP_AREA")
    index.loc_sa_for_sp = tell(out)
    for sp in index.stop_points:
        write_stop_area_idx(out,index,sp.stop_area.uri)

vjref_t = Struct('HH')
def export_vj_transitions(tdata,index,out):
    index.loc_vj_transfers_backward = tell(out)
    for jp in index.journey_patterns:
        for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
            out.write(vjref_t.pack(index.n_jp,0))
    index.loc_vj_transfers_forward = tell(out)
    for jp in index.journey_patterns:
        for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
            out.write(vjref_t.pack(index.n_jp,0))

def export_transfers(tdata,index,out):
    print "saving transfer stops (footpaths)"
    write_text_comment(out,"TRANSFER TARGET STOPS")
    index.loc_transfer_target_stop_points = tell(out)

    index.transfers_offsets = []
    offset = 0
    transfertimes = []
    stop_point_waittimes = {}
    for sp in index.stop_points:
        index.transfers_offsets.append(offset)
        if sp.uri not in index.connections_from_stop_point:
            continue
        for conn in index.connections_from_stop_point[sp.uri]:
            if conn.from_stop_point.uri == conn.to_stop_point.uri:
                stop_point_waittimes[conn.from_stop_point.uri] = conn.min_transfer_time
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
        writeshort(out,(int(transfer_time) >> 2))

    index.loc_stop_point_waittime = tell(out)
    for sp in index.stop_points:
        if sp.uri in stop_point_waittimes:
            writeshort(out,(int(stop_point_waittimes[sp.uri]) >> 2))
        else:
            writeshort(out,(int(MIN_WAITTIME) >> 2))

def export_stop_indices(tdata,index,out):
    print "saving stop indexes"
    write_text_comment(out,"STOP STRUCTS")
    index.loc_stop_points = tell(out)
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
    route_t = Struct('2I6H')
    jpp_offsets = index.offset_jpp
    trip_ids_offsets = index.vj_ids_offsets
    jp_attributes = []

    nroutes = len(index.journey_patterns)

    jp_n_jpp = []
    jp_n_vj = []
    routeidx_offsets = []
    jp_min_time = []
    jp_max_time = []
    for jp in index.journey_patterns:
        jp_n_jpp.append(len(jp.points))
        jp_n_vj.append(len(index.vehicle_journeys_in_journey_pattern[jp.uri]))
        jp_min_time.append(min([vj.departure_time+index.global_utc_offset for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]]) >> 2)
        jp_max_time.append(max([vj.departure_time+vj.timedemandgroup.points[-1].totaldrivetime+index.global_utc_offset
                                for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]]) >> 2)
        routeidx_offsets.append(index.idx_for_route_uri[jp.route.uri])

        jp_attributes.append(1 << jp.route.route_type)
    jp_t_fields = [jpp_offsets, trip_ids_offsets,jp_n_jpp, jp_n_vj,jp_attributes,routeidx_offsets,jp_min_time, jp_max_time]
    for l in jp_t_fields :
        # the extra last route is a sentinel so we can derive list lengths for the last true route.
        assert len(l) == nroutes
    for route in zip (*jp_t_fields) :
        # print route
        out.write(route_t.pack(*route));
    out.write(route_t.pack(jpp_offsets[-1]+1,0,0,0,0,0,0,0)) #Sentinel

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
    index.loc_platformcodes = write_list_of_strings(out,index,[sp.platformcode or '' for sp in index.stop_points])

def export_stop_pointnames(tdata,index,out):
    print "writing out locations for stopnames"
    write_text_comment(out,"STOP POINT NAMES")
    index.loc_stop_nameidx = write_list_of_strings(out,index,[sp.name or '' for sp in index.stop_points])

def export_stop_areanames(tdata,index,out):
    print "writing out locations for stopareas"
    write_text_comment(out,"STOP AREA NAMES")
    index.loc_stop_areaidx = write_list_of_strings(out,index,[sa.name or '' for sa in index.stop_areas])

def export_operators(tdata,index,out):
    print "writing out opreators to string pool"
    write_text_comment(out,"OPERATOR IDS")
    index.loc_operator_ids = write_list_of_strings(out,index,[op.uri or '' for op in index.operators])
    write_text_comment(out,"OPERATOR NAMES")
    index.loc_operator_names = write_list_of_strings(out,index,[op.name or '' for op in index.operators])
    write_text_comment(out,"OPERATOR URLS")
    index.loc_operator_urls = write_list_of_strings(out,index,[op.url or '' for op in index.operators])

def export_commercialmodes(tdata,index,out):
    print "writing out commercial_mode to string table"
    write_text_comment(out,"CCMODE IDS")
    index.loc_commercialmode_ids = write_list_of_strings(out,index,[cc.uri or '' for cc in index.commercial_modes])
    write_text_comment(out,"CCMODE NAMES")
    index.loc_commercialmode_names = write_list_of_strings(out,index,[cc.name or '' for cc in index.commercial_modes])
    index.loc_commercial_mode_for_jp = tell(out)
    for jp in index.journey_patterns:
        writeshort(out,index.idx_for_commercial_mode_uri[jp.commercial_mode.uri])

def export_physicalmodes(tdata,index,out):
    print "writing out commercial_mode to string table"
    write_text_comment(out,"CCMODE IDS")
    index.loc_physicalmode_ids = write_list_of_strings(out,index,[cc.uri or '' for cc in index.physical_modes])
    write_text_comment(out,"CCMODE NAMES")
    index.loc_physicalmode_names = write_list_of_strings(out,index,[cc.name or '' for cc in index.physical_modes])
    index.loc_physical_mode_for_line = tell(out)
    for l in index.lines:
        writeshort(out,index.idx_for_physical_mode_uri[l.physical_mode.uri])

def export_stringpool(tdata,index,out):
    print "writing out stringpool"
    write_text_comment(out,"STRINGPOOL")
    index.loc_stringpool = tell(out)
    written_length = 0
    for string in index.strings:
        out.write(string+'\0')
        written_length += len(string) + 1
    assert written_length == index.string_length

def export_linecodes(tdata,index,out):
    write_text_comment(out,"LINE CODES")
    index.loc_line_codes = write_list_of_strings(out,index,[line.code or '' for line in index.lines])

def export_linecolors(tdata,index,out):
    write_text_comment(out,"LINE COLOR")
    index.loc_line_color = write_list_of_strings(out,index,[line.color or '' for line in index.lines])
    write_text_comment(out,"LINE COLOR_TEXT")
    index.loc_line_color_text = write_list_of_strings(out,index,[line.color_text or '' for line in index.lines])

def export_linenames(tdata,index,out):
    write_text_comment(out,"LINE NAMES")
    index.loc_line_names = write_list_of_strings(out,index,[line.name or '' for line in index.lines])

def export_line_uris(tdata,index,out):
    print "writing line ids to string table"
    write_text_comment(out,"LINE IDS")
    index.loc_line_uris = write_list_of_strings(out,index,[line.uri for line in index.lines])

def export_sp_uris(tdata,index,out):
    print "writing out sorted stop_point ids to string point list"
    # stopid index was several times bigger than the string table. it's probably better to just store fixed-width ids.
    write_text_comment(out,"STOP_POINT IDS")
    index.loc_stop_point_uris = write_list_of_strings(out,index,[sp.uri for sp in index.stop_points])

def export_sa_uris(tdata,index,out):
    print "writing out sorted stop_area ids to string point list"
    write_text_comment(out,"STOP_AREA IDS")
    index.loc_stop_area_uris = write_list_of_strings(out,index,[sa.uri for sa in index.stop_areas])

def export_sa_timezones(tdata,index,out):
    write_text_comment(out,"STOP_AREA TIMEZONES")
    index.loc_stop_area_timezones = write_list_of_strings(out,index,[sa.timezone for sa in index.stop_areas])

def export_vj_time_offsets(tdata,index,out):
     print 'Timetable offset from UTC'+str(index.global_utc_offset)
     index.loc_vj_time_offsets = tell(out)
     for jp in index.journey_patterns:
         for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
             writesignedbyte(out,(index.global_utc_offset-vj.utc_offset)/60/15) # n * 15 minutes

def export_vj_uris(tdata,index,out):
     all_vj_ids = [] 
     for jp in index.journey_patterns:
         for vj in index.vehicle_journeys_in_journey_pattern[jp.uri]:
             all_vj_ids.append(vj.uri)
     index.n_vj = len(all_vj_ids)
     print "writing trip ids to string table" 
     # note that trip_ids are ordered by departure time within trip bundles (routes), which are themselves in arbitrary order. 
     write_text_comment(out,"VJ IDS")
     index.loc_vj_uris = write_list_of_strings(out,index,all_vj_ids)
     index.n_vj = len(all_vj_ids)

def export_routes(tdata,index,out):
    index.loc_line_for_route = tell(out)
    for r in index.routes:
        writeshort(out,index.idx_for_line_uri[r.line.uri])

def export_lines(tdata,index,out):
    index.loc_operator_for_line = tell(out)
    for l in index.lines:
        writebyte(out,index.idx_for_operator_uri[l.operator.uri])

def write_header (out,index) :
    """ Write out a file header containing offsets to the beginning of each subsection.
    Must match struct transit_data_header in transitdata.c """
    out.seek(0)
    htext = "TTABLEV4"

    packed = struct_header.pack(htext,
        index.calendar_start_time,
        index.timezone,
        index.global_utc_offset,
        index.n_days, # n_days
        index.n_stops, # n_stops
        len(index.stop_areas), #n_stop_areas
        index.n_stops, # n_stop_attributes
        index.n_stops, # n_stop_point_coords
        len(index.stop_areas), # n_stop_area_coords
        len(index.stop_points), # n_sa_for_ap
        index.n_jp, # n_routes
        index.n_jpp, # n_jpp
        index.n_jpp, # n_jpp_attributes
        index.n_jpp, # n_jpp_headsigns
        index.n_tpp, # n_stop_times
        index.n_vj, # n_vjs
        index.n_jpp_at_sp, # n_stop_routes
        index.n_connections, #n_transfer_target_stop
        index.n_connections, #n_transfer_dist_meters
        index.n_vj, #n_trip_active
        index.n_jp, # n_jp_active
        index.n_stops, # n_platformcodes
        len(index.stop_points), # n_stop_nameidx
        len(index.stop_areas), # n_stop_nameidx
        len(index.operators), # n_operator_id
        len(index.operators), # n_operator_names
        len(index.operators), # n_operator_urls
        len(index.commercial_modes), # n_commercialmode_id
        len(index.commercial_modes), # n_commercialmode_names
        len(index.physical_modes), # n_physicalmode_id
        len(index.physical_modes), # n_physicalmode_names
        index.string_length, # n_string_pool (length of the object)
        len(index.lines), # n_line_codes
        len(index.lines), # n_line_ids
        len(index.lines), # n_line_colors
        len(index.lines), # n_line_colors_text
        len(index.lines), # n_line_names
        len(index.stop_points), # n_stop_point_ids
        len(index.stop_areas), # n_stop_area_ids
        len(index.stop_areas), # n_stop_area_timezones
        index.n_vj, # n_vj_time_offsets
        index.n_vj, # n_vj_ids
        len(index.routes), #n_line_for_route
        len(index.lines), #n_operator_for_line
        len(index.journey_patterns), #n_commerical_mode_for_jp
        len(index.lines), #n_commerical_mode_for_line
        index.n_vj, #n_vj_transfers_backward
        index.n_vj, #n_vj_transfers_foward
        len(index.stop_points), #n_stop_point_waittime

        index.loc_stop_points,
        index.loc_stop_point_attributes,
        index.loc_stop_point_coords,
        index.loc_journey_patterns,
        index.loc_journey_pattern_points,
        index.loc_journey_pattern_point_attributes, 
        index.loc_journey_pattern_point_headsigns,
        index.loc_timedemandgroups,
        index.loc_vehicle_journeys,
        index.loc_jp_at_sp,
        index.loc_transfer_target_stop_points,
        index.loc_transfer_dist_meters,
        index.loc_stop_point_waittime,
        index.loc_vj_transfers_backward,
        index.loc_vj_transfers_forward,
        index.loc_vj_active,
        index.loc_jp_active,
        index.loc_platformcodes,
        index.loc_stop_nameidx,
        index.loc_stop_areaidx,
        index.loc_line_for_route,
        index.loc_operator_for_line,
        index.loc_operator_ids,
        index.loc_operator_names,
        index.loc_operator_urls,
        index.loc_commercialmode_ids,
        index.loc_commercialmode_names,
        index.loc_commercial_mode_for_jp,
        index.loc_physicalmode_ids,
        index.loc_physicalmode_names,
        index.loc_physical_mode_for_line,
        index.loc_stringpool,
        index.loc_line_codes,
        index.loc_line_names,
        index.loc_line_uris,
        index.loc_line_color,
        index.loc_line_color_text,
        index.loc_stop_point_uris,
        index.loc_stop_area_uris,
        index.loc_stop_area_timezones,
        index.loc_vj_time_offsets,
        index.loc_vj_uris,
        index.loc_stop_area_coords,
        index.loc_sa_for_sp,
    )
    out.write(packed)

struct_header = Struct('8sQi91I')

def export(tdata):
    index = make_idx(tdata)
    index.n_days = NUMBER_OF_DAYS
    index.calendar_start_time = (tdata.validfrom.toordinal() - datetime.date(1970, 1, 1).toordinal()) * 24*60*60
    print index.calendar_start_time
    index.n_stops = len(index.stop_points)
    index.n_jp = len(index.journey_patterns)
    index.timezone = index.put_string(tdata.timezone)
    out = open('timetable4.dat','wb')
    out.seek(struct_header.size)

    export_sp_coords(tdata,index,out)
    export_journey_pattern_point_stop(tdata,index,out)
    export_journey_pattern_point_attributes(tdata,index,out)
    export_timedemandgroups(tdata,index,out)
    export_vj_in_jp(tdata,index,out)
    export_jpp_at_sp(tdata,index,out)
    export_transfers(tdata,index,out)
    export_vj_transitions(tdata,index,out)
    export_stop_indices(tdata,index,out)
    export_stop_point_attributes(tdata,index,out)
    export_jp_structs(tdata,index,out)
    export_vj_validities(tdata,index,out)
    export_jp_validities(tdata,index,out)
    export_platform_codes(tdata,index,out)
    export_sa_coords(tdata,index,out)
    export_sa_for_sp(tdata,index,out)
    export_stop_pointnames(tdata,index,out)
    export_stop_areanames(tdata,index,out)
    export_operators(tdata,index,out)
    export_commercialmodes(tdata,index,out)
    export_physicalmodes(tdata,index,out)
    export_routes(tdata,index,out)
    export_lines(tdata,index,out)
    export_linecodes(tdata,index,out)
    export_linenames(tdata,index,out)
    export_linecolors(tdata,index,out)
    export_journey_pattern_point_headsigns(tdata,index,out)
    export_line_uris(tdata,index,out)
    export_sp_uris(tdata,index,out)
    export_sa_uris(tdata,index,out)
    export_sa_timezones(tdata,index,out)
    export_vj_time_offsets(tdata,index,out)
    export_vj_uris(tdata,index,out)
    export_stringpool(tdata,index,out)
    print "reached end of timetable file"
    write_text_comment(out,"END TTABLEV4")
    index.loc_eof = tell(out)
    print "rewinding and writing header... ",
    write_header(out,index)

    out.flush()
    out.close()

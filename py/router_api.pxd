from libc.stdint cimport uint64_t, uint32_t, uint16_t, uint8_t, int16_t, int32_t, int64_t


cdef int RRRR_MAX_ROUNDS = 20

ctypedef uint8_t bool

cdef extern from "string.h":
    char* strstr(const char *haystack, const char *needle)


cdef extern from "geometry.h":
    ctypedef latlon latlon_t
    cdef:
        struct latlon:
            float lat
            float lon    

cdef extern from "hashgrid.h":
    cdef:
        struct HashGrid:
            pass
    ctypedef HashGrid hashGrid_t

cdef extern from "util.h":
    ctypedef uint16_t rtime_t

cdef extern from "gtfs-realtime.pb-c.h":
    ctypedef _TransitRealtime__FeedMessage TransitRealtime__FeedMessage
    cdef:
        struct _TransitRealtime__FeedMessage:
            pass

cdef extern from "bitset.h":
    cdef:
        struct BitSet:
            uint32_t  capacity
            uint64_t* chunks
            uint32_t  nchunks
    ctypedef BitSet bitset_s


cdef extern from "tdata.h":
    ctypedef uint32_t calendar_t

    ctypedef stop stop_t
    cdef:
        struct stop:
            uint32_t stop_routes_offset
            uint32_t transfers_offset

    ctypedef route route_t
    cdef:
        struct route:
            uint32_t route_stops_offset
            uint32_t trip_ids_offset
            uint32_t headsign_offset
            uint16_t n_stops
            uint16_t n_trips
            uint16_t attributes
            uint16_t agency_index
            uint16_t shortname_index
            uint16_t productcategory_index
            rtime_t  min_time
            rtime_t  max_time

    ctypedef trip trip_t
    cdef:
        struct trip:
            uint32_t stop_times_offset
            rtime_t  begin_time
            int16_t  realtime_delay

    ctypedef stoptime stoptime_t
    cdef:
        struct stoptime:
            rtime_t arrival
            rtime_t departure


    ctypedef tdata tdata_t
    cdef:
        struct tdata:
            void* base
            uint32_t size
            uint64_t calendar_start_time
            calendar_t dst_active
            uint32_t n_stops
            uint32_t n_routes
            uint32_t n_trips
            stop_t *stops
            uint8_t *stop_attributes
            route_t *routes
            uint32_t *route_stops
            uint8_t  *route_stop_attributes
            stoptime_t *stop_times
            trip_t *trips
            uint32_t *stop_routes
            uint32_t *transfer_target_stops
            uint8_t  *transfer_dist_meters
            latlon_t *stop_coords
            uint32_t platformcode_width
            char *platformcodes
            char *stop_names
            uint32_t *stop_nameidx
            uint32_t agency_id_width
            char *agency_ids
            uint32_t agency_name_width
            char *agency_names
            uint32_t agency_url_width
            char *agency_urls
            char *headsigns
            uint32_t route_shortname_width
            char *route_shortnames
            uint32_t productcategory_width
            char *productcategories
            calendar_t *trip_active
            calendar_t *route_active
            uint8_t *trip_attributes
            uint32_t route_id_width
            char *route_ids
            uint32_t stop_id_width
            char *stop_ids
            uint32_t trip_id_width
            char *trip_ids
            TransitRealtime__FeedMessage *alerts

    void tdata_load(char* filename, tdata_t*)
    void tdata_close(tdata_t*)


cdef extern from "router.h":

    ctypedef router_state router_state_t
    cdef:
        struct router_state:
            uint32_t back_stop
            uint32_t back_route
            uint32_t back_trip
            rtime_t  time
            rtime_t  board_time
            uint32_t walk_from
            rtime_t  walk_time

    ctypedef router router_t
    cdef:
        struct router:
            tdata_t tdata
            rtime_t *best_time
            router_state_t *states
            BitSet *updated_stops
            BitSet *updated_routes

    ctypedef router_request router_request_t
    cdef:
        struct router_request:
            uint32_t fr
            uint32_t to
            uint32_t via
            uint32_t start_trip_route
            uint32_t start_trip_trip
            rtime_t time
            rtime_t time_cutoff
            double walk_speed
            uint8_t walk_slack
            uint32_t max_transfers
            uint32_t day_mask
            uint8_t mode
            uint8_t trip_attributes
            uint8_t optimise
            uint32_t n_banned_routes
            uint32_t n_banned_stops
            uint32_t n_banned_stops_hard
            uint32_t n_banned_trips
            uint32_t banned_route
            uint32_t banned_stop
            uint32_t banned_trip_route
            uint32_t banned_trip_offset
            uint32_t banned_stop_hard

    cdef struct Leg:
            uint32_t s0
            uint32_t s1
            rtime_t  t0
            rtime_t  t1
            uint32_t route
            uint32_t trip

    cdef struct Ininerary:
            uint32_t n_rides
            uint32_t n_legs
            Leg legs[20 * 2 + 1]

    cdef struct Plan:
            router_request_t req
            uint32_t n_itineraries
            Ininerary itineraries[10]

    

    void router_request_initialize(router_request_t* req)

    void router_setup(router_t*, tdata_t*)

    void router_request_randomize(router_request_t* req, tdata_t* tdata)

    void router_result_to_plan (Plan* plan, router_t *router, router_request_t* req)

    void router_teardown(router_t *router)


cdef extern from "parse.c":

    bool parse_request_from_qstring(router_request_t *req, tdata_t *tdata, HashGrid *hg, char *qstring)


cdef extern from "py_support.h":

    void trans_coord_and_init_grid(tdata_t tdata, hashGrid_t *hash_grid)

    char * find_multiple_connections(router_t *router, router_request_t *init_req, int n_conn)
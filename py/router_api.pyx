from libc.string    cimport strncpy
from libc.stdlib    cimport free
from libc.stdio     cimport printf

from cpython.string cimport PyString_FromString


def find_connection(qstring, NUM_CONNECTIONS):

    cdef router_t          router
    cdef tdata_t           tdata
    cdef router_request_t  req
    cdef char             *json_dump
    cdef int               i

    INPUT_FILE               = "timetable.dat"
    py_json_dump             = []

    tdata_load(INPUT_FILE, &tdata)          # Load graph to memmory
    router_setup(&router, &tdata)           # Load router

    req             = create_request(tdata, qstring)
    json_dump       = find_multiple_connections(&router, &req, NUM_CONNECTIONS)
    py_json_dump    = PyString_FromString(json_dump)

    #clean up...
    free(json_dump)				# It has been 'mallocated', therefore it has to be 'freed'
    tdata_close(&tdata)
    router_teardown(&router)

    return py_json_dump


cdef router_request_t create_request(tdata_t tdata, char qstring[]):

    cdef router_request_t  req
    cdef HashGrid          grid                # For looking up stops by location

    trans_coord_and_init_grid(tdata, &grid)	   # grid has to be initialized using tdata
    router_request_initialize(&req)
    
    parse_request_from_qstring(&req, &tdata, &grid, qstring) 
    return req

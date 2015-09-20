#include <Python.h>
#include "structmember.h"
#include "config.h"
#include "plan_render_otp.h"
#include "set.h"
#include "api.h"
#include "plan.h"
#include "router_request.h"

typedef struct {
    /* The object itself */
    PyObject_HEAD

    /* filename of timetable4.dat */
    PyObject *timetable;

    /* json.loads function */
    PyObject *json;

    /* interface to a loaded timetable */
    tdata_t tdata;

    /* interface to the router instance */
    router_t router;
} Raptor;

static void
Raptor_dealloc (Raptor* self)
{
    /* free the memory used in the object */
    Py_XDECREF (self->timetable);
    router_teardown (&self->router);
    tdata_hashgrid_teardown (&self->tdata);
    tdata_close (&self->tdata);
    self->ob_type->tp_free ((PyObject*)self);
}

static PyObject *
Raptor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Raptor *self;
    PyObject *json;

    self = (Raptor *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->timetable = PyString_FromString("");
        if (self->timetable == NULL) {
            Py_DECREF(self);
            return NULL;
        }

        /* initialise the memory for the router */
        memset (&self->tdata,  0, sizeof(tdata_t));
        memset (&self->router, 0, sizeof(router_t));
    }

    /* import json */
    json = PyImport_ImportModule("json");

    /* json.loads() */
    self->json = PyObject_GetAttrString(json, "loads");

    return (PyObject *)self;
}

static int
Raptor_init(Raptor *self, PyObject *args, PyObject *kwds)
{
    PyObject *timetable=NULL, *tmp;

    {
        /* class Raptor: def __init__(timetable) */
        static char *kwlist[] = {"timetable", NULL};
        if (! PyArg_ParseTupleAndKeywords(args, kwds,
                                          "O", kwlist,
                                          &timetable)) {
            return -1;
        }
    }

    if (timetable) {
        tmp = self->timetable;
        Py_INCREF(timetable);
        self->timetable = timetable;
        Py_XDECREF(tmp);

        /* Initialise the journey planner */
        char *filename = PyString_AsString(self->timetable);
        if (! tdata_load(&self->tdata, filename) ||
            ! tdata_hashgrid_setup (&self->tdata) ||
            ! router_setup (&self->router, &self->tdata)) {
            return -1;
        }
    }

    return 0;
}


static PyMemberDef Raptor_members[] = {
    {"timetable", T_OBJECT_EX, offsetof(Raptor, timetable), 0,
     "Path to the location of timetable4.dat"},
    {NULL}  /* Sentinel */
};

static PyObject *
Raptor_stops(Raptor* self, PyObject *args, PyObject *keywords)
{
    spidx_t stop_index = (spidx_t) self->tdata.n_stop_points;
    PyObject *list = PyList_New(stop_index);

    while (stop_index) {
        const char *stop_name;
        stop_index--;
        stop_name = tdata_stop_point_name_for_index(&self->tdata, stop_index);
        PyList_SetItem (list, stop_index, PyString_FromString (stop_name));
    }

    return list;
}

static PyObject *
Raptor_route(Raptor* self, PyObject *args, PyObject *keywords)
{
    char *from_id = NULL, *from_sp_id = NULL,
         *to_id = NULL, *to_sp_id = NULL,
         *operator = NULL;
    time_t arrive = 0, depart = 0, epoch = 0;
    router_request_t req;
    plan_t plan;
    #define OUTPUT_LEN 100000
    char result_buf[OUTPUT_LEN];

    router_request_initialize (&req);

    {
        static char * list[] = { "from_id", "to_id",
                                 "from_sp_id", "to_sp_id",
                                 "from_latlon", "to_latlon",
                                 "from_idx", "to_idx",
                                 "from_sp_idx", "to_sp_idx",
                                 "arrive", "depart", "operator" };

        if ( !PyArg_ParseTupleAndKeywords(args, keywords, "|ssss(ff)(ff)HHHHlls",
                list, &from_id, &to_id,
                      &from_sp_id, &to_sp_id,
                      &req.from_latlon.lat, &req.from_latlon.lon,
                      &req.to_latlon.lat, &req.to_latlon.lon,
                      &req.from_stop_area, &req.to_stop_area,
                      &req.from_stop_point, &req.to_stop_point,
                      &arrive, &depart, &operator)) {
            return NULL;
        }
    }

    /* Validate input */
    if ( (from_id == NULL && from_sp_id == NULL &&
          req.from_stop_area == STOP_NONE && req.from_stop_point == STOP_NONE &&
          (req.from_latlon.lon == 0.0 && req.from_latlon.lat == 0.0 )) ||
         (to_id == NULL &&   to_sp_id == NULL &&
          req.to_stop_area == STOP_NONE && req.to_stop_point == STOP_NONE &&
          (  req.to_latlon.lon == 0.0 &&   req.to_latlon.lat == 0.0 )) ||
         ( arrive == 0    &&     depart == 0)) {
        PyErr_SetString(PyExc_AttributeError, "Missing mandatory input");
        return NULL;
    }

    /* Temporal related input */
    if (arrive != 0) {
        req.arrive_by = true;
        epoch = arrive;
    } else {
        req.arrive_by = false;
        epoch = depart;
    }

    router_request_from_epoch (&req, &self->tdata, epoch);
    if (req.time_rounded && ! (req.arrive_by)) {
        req.time++;
    }
    req.time_rounded = false;

    if (req.arrive_by) {
        req.time_cutoff = 0;
    } else {
        req.time_cutoff = UNREACHED;
    }


    /* Spatial related input */
    if (from_id) {
        req.from_stop_area = tdata_stop_areaidx_by_stop_area_id (&self->tdata, from_id, 0);
    }

    if (from_sp_id) {
        req.from_stop_point = tdata_stop_pointidx_by_stop_point_id (&self->tdata, from_sp_id, 0);
    }

    if (to_id) {
        req.to_stop_area = tdata_stop_areaidx_by_stop_area_id (&self->tdata, to_id, 0);
    }

    if (to_sp_id) {
        req.to_stop_point = tdata_stop_pointidx_by_stop_point_id (&self->tdata, to_sp_id, 0);
    }

    /* Filtering related input */
    if (operator) {
        opidx_t op_idx = tdata_operator_idx_by_operator_name (&self->tdata, operator, 0);
        while (op_idx != OP_NONE)
        {
            set_add_uint8 (req.operators, &req.n_operators, RRRR_MAX_FILTERED_OPERATORS, op_idx);
            op_idx = tdata_operator_idx_by_operator_name (&self->tdata, operator, op_idx + 1);
        }
    }

    plan_init (&plan);

    if (!router_route_all_departures (&self->router, &req, &plan)) {
        PyErr_SetString(PyExc_AttributeError, "router");
        return NULL;
    }

    plan.req = req;
    plan_render_otp (&plan, &self->tdata, result_buf, OUTPUT_LEN);

    /* return json.loads(result_buf) */
    return PyObject_CallFunctionObjArgs(self->json, PyString_FromString (result_buf), NULL);
}

static PyMethodDef Raptor_methods[] = {
    {"route", (PyCFunction)Raptor_route, METH_VARARGS | METH_KEYWORDS,
     "Return a JSON-OTP formatted set of itineraries"
    },
    {"stops", (PyCFunction)Raptor_stops, METH_NOARGS,
     "Return a List of stops from the timetable"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject RaptorType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "rrrr.Raptor",              /*tp_name*/
    sizeof(Raptor),             /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)Raptor_dealloc, /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Raptor objects",           /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    Raptor_methods,             /* tp_methods */
    Raptor_members,             /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    (initproc)Raptor_init,      /* tp_init */
    0,                          /* tp_alloc */
    Raptor_new,                 /* tp_new */
};

static PyMethodDef module_methods[] = {
    {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC  /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initrrrr (void)
{
    PyObject* m;

    if (PyType_Ready (&RaptorType) < 0)
        return;

    m = Py_InitModule3 ("rrrr", module_methods,
                        "Public transport Journey Planner.");

    if (m == NULL)
      return;

    Py_INCREF (&RaptorType);
    PyModule_AddObject (m, "Raptor", (PyObject *)&RaptorType);
}

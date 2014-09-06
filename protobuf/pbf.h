#ifndef PBF_H_INCLUDED
#define PBF_H_INCLUDED

#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"

typedef struct {
    void (*way) (OSMPBF__Way*);
    void (*node) (OSMPBF__Node*);
} osm_callbacks_t;

void scan_pbf(const char *filename, osm_callbacks_t *callbacks);

#endif /* PBF_H_INCLUDED */

#ifndef PBF_H_INCLUDED
#define PBF_H_INCLUDED

#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"

typedef struct {
    void (*way) (OSMPBF__Way*, ProtobufCBinaryData *string_table);
    void (*node) (OSMPBF__Node*, ProtobufCBinaryData *string_table);
} osm_callbacks_t;

void scan_pbf(const char *filename, osm_callbacks_t *callbacks);

#endif /* PBF_H_INCLUDED */

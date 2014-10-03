#ifndef PBF_H_INCLUDED
#define PBF_H_INCLUDED

#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"
#include <stdio.h>

//TODO why can't we just pass these as separate params?
typedef struct {
    void (*way) (OSMPBF__Way*, ProtobufCBinaryData *string_table);
    void (*node) (OSMPBF__Node*, ProtobufCBinaryData *string_table);
} osm_callbacks_t;

/* READ */
void scan_pbf(const char *filename, osm_callbacks_t *callbacks);

/* WRITE */
void write_pbf_begin (FILE *out);
void write_pbf_way(uint64_t way_id, int64_t *refs, uint8_t *coded_tags);
void write_pbf_node(uint64_t node_id, double lat, double lon, uint8_t *coded_tags);
void write_pbf_flush();

#endif /* PBF_H_INCLUDED */

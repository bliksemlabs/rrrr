/* pbf-write.c */
#include "pbf.h"

#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <arpa/inet.h>
#include "zlib.h"
#include "tags.h"

/*
    We are using the c protobuf compiler https://github.com/protobuf-c/protobuf-c/
    With protobuf definition files from the Java/C++ project https://github.com/scrosby/OSM-binary

    protobuf-c provides ProtobufCBuffer self-expanding buffers.
    However the PBF spec also gives maximum sizes for chunks:
    "The uncompressed length of a Blob *should* be less than 16 MiB (16*1024*1024 bytes) 
    and *must* be less than 32 MiB." So we can just make the buffers that size.
    
    A PBF file is chunked, and can be produced and consumed in a streaming fashion.
    However, many applications require the nodes to arrive before the ways.    
    A PBF file is a series of blobs. Each blob is preceded by its size in network byte order.

The PBF format is a repeating sequence of:
  32-bit length of the BlobHeader message in network byte order
  serialized BlobHeader message
  serialized Blob message (size is given in the BlobHeader)

The BlobHeader gives the type of the following Blob, a string which can be OSMHeader or OSMData.
You must send one OSMHeader blob before sending the OSMData blobs. 
Each OSMData blob contains some optionally zlib-compressed bytes, which contain one PrimitiveBlock 
(an independently decompressible block of 8k entities).

We should be able to provide the DenseNodes, and perhaps Sort.Type_then_ID features.
*/

static FILE *out = NULL;

/* Buffers for protobuf packed and zlib compresed data. Max sizes are given by the PBF spec. */
static uint8_t blob_buffer[16*1024*1024];
static uint8_t zlib_buffer[16*1024*1024];
static uint8_t blob_header_buffer[64*1024];

/* 
  Provide an uncompressed payload. 
  The first blob in the stream should be a header_blob. 
  Its payload is a packed HeaderBlock rather than a packed PrimitiveBlock.
*/
static void 
write_one_blob (uint8_t *payload, uint64_t payload_len, char *type, FILE *out) {

    /* Compress the payload. */
    ProtobufCBinaryData zbd;
    zbd.data = zlib_buffer;
    zbd.len = sizeof(zlib_buffer);
    if (compress(zlib_buffer, &(zbd.len), payload, payload_len) != Z_OK) {
        printf("Error while compressing PBF blob payload.");
        exit(-1);
    }

    /* Create and pack the blob itself. */
    OSMPBF__Blob blob;
    osmpbf__blob__init(&blob);
    blob.zlib_data = zbd;
    blob.has_zlib_data = true;
    blob.raw_size = payload_len;  // spec: "Only set when compressed, to the uncompressed size"
    blob.has_raw_size = true;
    size_t blob_packed_length = osmpbf__blob__pack(&blob, blob_buffer);

    /* Make a header for this blob. */
    OSMPBF__BlobHeader blob_header;
    osmpbf__blob_header__init(&blob_header);
    blob_header.type = type;
    blob_header.datasize = blob_packed_length; // spec: "serialized size of the subsequent Blob message"
    // TODO check packed size before packing
    size_t blob_header_packed_length = osmpbf__blob_header__pack(&blob_header, blob_header_buffer);

    /* Write the basic recurring PBF unit: blob header length, blob header, blob. */
    uint32_t bhpl_net = htonl(blob_header_packed_length);
    fwrite(&bhpl_net, 4, 1, out);
    fwrite(&blob_header_buffer, blob_header_packed_length, 1, out);
    fwrite(&blob_buffer, blob_packed_length, 1, out);

    printf("%s blob written:\n", type);
    printf("payload length (raw)     %ld\n", payload_len);
    printf("payload length (zipped)  %ld\n", zbd.len);
    printf("packed length of body    %zd\n", blob_packed_length);
    printf("packed length of header  %zd\n", blob_header_packed_length);
    
}

/* Blocks of PBF Node and Way structs for creating primitive blocks. */
#define PBF_BLOCK_SIZE 8000
static OSMPBF__Node node_block[PBF_BLOCK_SIZE];
static OSMPBF__Node *node_block_p[PBF_BLOCK_SIZE];
static OSMPBF__Way way_block[PBF_BLOCK_SIZE];
static OSMPBF__Way *way_block_p[PBF_BLOCK_SIZE];
static uint32_t node_block_count; // number of nodes now stored in block
static uint32_t way_block_count; // number of ways now stored in block

/* protobuf-c API takes arrays of pointers to structs. Initialize those arrays once at startup. */
static void initialize_pointer_arrays() {
    OSMPBF__Way  *wp = &(way_block[0]); 
    OSMPBF__Node *np = &(node_block[0]);
    for (int i = 0; i < PBF_BLOCK_SIZE; i++) {
        way_block_p[i]  = wp++; 
        node_block_p[i] = np++; 
    }
    node_block_count = 0;
    way_block_count = 0;
}

/* 
  Resets the node and way arrays, as well as the string table.
  We always produce one pgroup per pblock, one pblock per data blob.
static void reset_blocks() {
    node_block_count = 0;
    way_block_count = 0;
}
*/

static uint8_t payload_buffer[32*1024*1024];

static void write_pbf_header_blob () {

    /* First blob is a header blob (payload is a HeaderBlock). */
    OSMPBF__HeaderBlock hblock;
    osmpbf__header_block__init(&hblock);
    char* features[2] = {"OsmSchema-V0.6", "DenseNodes"};
    hblock.required_features = features;
    hblock.n_required_features = 2;
    hblock.writingprogram = "RRRR_COSM";
    size_t payload_len = osmpbf__header_block__pack(&hblock, payload_buffer);
    write_one_blob (payload_buffer, payload_len, "OSMHeader", out);

}

/* PUBLIC Begin writing a PBF file, and perform some setup. */
void write_pbf_begin (FILE *out_file) {
    out = out_file;
    initialize_pointer_arrays();
    write_pbf_header_blob();
}

/* Write one data blob containing any buffered ways and nodes. */
static void write_pbf_data_blob (bool nodes, bool ways) {

    OSMPBF__PrimitiveBlock pblock;
    osmpbf__primitive_block__init(&pblock);
    OSMPBF__StringTable stable;
    osmpbf__string_table__init(&stable);
    OSMPBF__PrimitiveGroup pgroup;
    osmpbf__primitive_group__init(&pgroup);

    /* Payload is a PrimitiveBlock containing one PrimitiveGroup of 8k ways. */
    OSMPBF__PrimitiveGroup *pgroups[1];
    pgroups[0] = &pgroup;
    pblock.primitivegroup = pgroups;
    pblock.n_primitivegroup = 1;
    pblock.stringtable = &stable;
    // We don't use any of the other block-level features (offsets, granularity, etc.)

    if (nodes && node_block_count > 0) {
        printf("Writing data blob containing nodes.\n");
        pgroup.nodes = node_block_p;
        pgroup.n_nodes = node_block_count;
        node_block_count = 0;
    } 
    if (ways && way_block_count > 0) {
        printf("Writing data blob containing ways.\n");
        pgroup.ways = way_block_p;
        pgroup.n_ways = way_block_count;
        way_block_count = 0;
    }

    size_t payload_len = osmpbf__primitive_block__pack(&pblock, payload_buffer);
    write_one_blob (payload_buffer, payload_len, "OSMData", out);

}

// TODO a function that gets a pointer to the next available way struct (avoid copying)
// TODO clearly there can be only one file at a time, just make that a static variable

// TODO hashtable from strings to ints, with ints assigned sequentially
// Then string table can be produced at the end.

/* PUBLIC Write one way in a buffered fashion, writing out a blob as needed (every 8k objects). */
void write_pbf_way(OSMPBF__Way *way) {
    way_block[way_block_count++] = *way;
    if (way_block_count == PBF_BLOCK_SIZE) {
        write_pbf_data_blob(false, true);
    }
}

/* PUBLIC Write one node in a buffered fashion, writing out a blob as needed (every 8k objects). */
void write_pbf_node(uint64_t node_id, double lat, double lon, int8_t *coded_tags) {
    OSMPBF__Node *node = &(node_block[node_block_count]);
    osmpbf__node__init(node);
    node->id = node_id;
    node->lat = (int64_t)(lat * 1000000);
    node->lon = (int64_t)(lat * 1000000);
    uint32_t n_tags = 0;
    char *t = (char*) coded_tags;
    KeyVal kv;
    while (*t != INT8_MAX) {
        t += decode_tag(t, &kv);
        n_tags++;
    }
    node_block_count++;
    if (node_block_count == PBF_BLOCK_SIZE) {
        write_pbf_data_blob(true, false);
    }
}

/* PUBLIC Write out a block for any objects remaining in the buffer. Call at the end of output. */
void write_pbf_flush() {
    if (node_block_count > 0) {
        write_pbf_data_blob(true, false);
    }
    if (way_block_count > 0) {
        write_pbf_data_blob(false, true);
    }
}



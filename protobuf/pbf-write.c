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
#include "dedup.h"

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

    /*
    printf("%s blob written:\n", type);
    printf("payload length (raw)     %ld\n", payload_len);
    printf("payload length (zipped)  %ld\n", zbd.len);
    printf("packed length of body    %zd\n", blob_packed_length);
    printf("packed length of header  %zd\n", blob_header_packed_length);
    */

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

/* Free all dynamically allocated arrays, and reset the block length to zero. */
static void reset_node_block() {
    node_block_count = 0;
}

/* Free all dynamically allocated node reference arrays, and reset the block length to zero. */
static void reset_way_block() {
    for (int w = 0; w < node_block_count; w++) {
        free(way_block[w].refs); // allocated in write_pbf_way
    }
    way_block_count = 0;
}

/* An array of string table indexes to store all the keys and vals in a block. */
#define MAX_KEYS_VALS 1024 * 1024
static uint32_t kv_buff[MAX_KEYS_VALS];
static size_t kv_n = 0;

/* Allocate a chunk of n string pointers for tag keys or values. */
static uint32_t *kv_alloc(size_t n) {
    if (kv_n + n > MAX_KEYS_VALS) {
        printf("too many key/val string table references in a block.\n");
        return NULL;
    }
    uint32_t *ret = &(kv_buff[kv_n]);
    kv_n += n;
    return ret;
}

/* Free all allocated tag key/value string pointers. */
static void kv_free_all() {
    kv_n = 0;
}

/* Used to hold the packed version of a header block or data block, passed to the blob encoder. */
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

/* Write one data blob containing any buffered ways or nodes. */
static void write_pbf_data_blob (bool nodes, bool ways) {

    OSMPBF__PrimitiveBlock pblock;
    osmpbf__primitive_block__init(&pblock);
    OSMPBF__PrimitiveGroup pgroup;
    osmpbf__primitive_group__init(&pgroup);

    /* Payload is a PrimitiveBlock containing one PrimitiveGroup of 8k elements. */
    OSMPBF__PrimitiveGroup *pgroups[1];
    pgroups[0] = &pgroup;
    pblock.primitivegroup = pgroups;
    pblock.n_primitivegroup = 1;
    pblock.stringtable = Dedup_string_table(); // table will be deallocated by Dedup_clear call
    // We don't use any of the other block-level features (offsets, granularity, etc.)

    if (nodes && node_block_count > 0) {
        printf("Writing data blob containing nodes.\n");
        pgroup.nodes = node_block_p;
        pgroup.n_nodes = node_block_count;
    } 
    if (ways && way_block_count > 0) {
        printf("Writing data blob containing ways.\n");
        pgroup.ways = way_block_p;
        pgroup.n_ways = way_block_count;
    }

    size_t payload_len = osmpbf__primitive_block__pack(&pblock, payload_buffer);
    write_one_blob (payload_buffer, payload_len, "OSMData", out);

    /* We always produce one pgroup per pblock, one pblock per data blob. */
    //Dedup_print();
    Dedup_clear(); // restart a new string table for each blob
    if (nodes) reset_node_block();
    if (ways) reset_way_block();
    kv_free_all(); // FIXME this is freeing the kv table even when only one of nodes/ways has been written...

}

/* PUBLIC Begin writing a PBF file, and perform some setup. */
void write_pbf_begin (FILE *out_file) {
    out = out_file;
    initialize_pointer_arrays();
    write_pbf_header_blob();
    Dedup_init(); 
}

/* PUBLIC Write out a block for any objects remaining in the buffer. Call at the end of output. */
void write_pbf_flush() {
    if (node_block_count > 0 || way_block_count > 0) {
        write_pbf_data_blob(true, true);
    }
}


// TODO a function that gets a pointer to the next available way struct (avoid copying)
// TODO clearly there can be only one file at a time, just make that a static variable

/* Return the number of tags loaded. Save string table indexes into the arrays in the last two params. */
static size_t load_tags(uint8_t *coded_tags, /*OUT*/ uint32_t **keys, /*OUT*/ uint32_t **vals) {

    /* First count tags. */
    size_t n_tags = 0;
    char *t = (char*) coded_tags;
    while (*t != INT8_MAX) { 
        KeyVal kv;
        t += decode_tag(t, &kv);
        n_tags++;
    } // FIXME there should be a simpler "count tags" function, or we should prefix the list with a varint length.

    /* Then copy string table indexes of keys and values into a subsection of the kv buffer. */
    uint32_t *kbuf = kv_alloc(n_tags);
    uint32_t *vbuf = kv_alloc(n_tags);
    n_tags = 0;
    t = (char*) coded_tags;
    while (*t != INT8_MAX) {
        KeyVal kv;
        t += decode_tag(t, &kv);
        kbuf[n_tags] = Dedup_dedup(kv.key);
        vbuf[n_tags] = Dedup_dedup(kv.val);
        n_tags++;
    }

    /* Tell the caller where we put the string table indexes for the keys and vals. */
    *keys = kbuf;
    *vals = vbuf;
    return n_tags;

}


/* PUBLIC Write one way in a buffered fashion, writing out a blob as needed (every 8k objects). */
void write_pbf_way(uint64_t way_id, int64_t *refs, uint8_t *coded_tags) {

    /* 
      We must copy the refs list, and cannot use it directly: 
      It contains negative sentinel values and is not delta coded.
      First, count number of node refs in this way.
    */
    size_t n_refs = 1;
    for (int64_t *r = refs; *r >= 0; r++) {
        n_refs++;
    }

    /* Delta code refs, copying them into a dynamically allocated buffer. */
    int64_t *refs_buf = malloc(n_refs * sizeof(int64_t)); // deallocated in reset_way_block
    if (refs_buf == NULL) exit (-1);
    int64_t prev_ref = 0; 
    for (int i = 0; i < n_refs; i++) {
        int64_t ref = refs[i];
        if (ref < 0) ref = -ref; // should only happen on the last ref in the list
        refs_buf[i] = ref - prev_ref; // refs within a way are delta coded
        prev_ref = ref;
    }

    /* Grab an unused OSMPBF Way struct from the block. */
    OSMPBF__Way *way = &(way_block[way_block_count]);
    osmpbf__way__init(way);
    way->id = way_id;
    way->refs = refs_buf;
    way->n_refs = n_refs;

    /* Load Tags */
    size_t n_tags = load_tags(coded_tags, &(way->keys), &(way->vals));
    way->n_keys = n_tags;
    way->n_vals = n_tags;

    /* Write out a block if we've filled the buffer. */
    way_block_count++;
    if (way_block_count == PBF_BLOCK_SIZE) {
        write_pbf_data_blob(false, true);
    }
}

/* PUBLIC Write one node in a buffered fashion, writing out a blob as needed (every 8k objects). */
void write_pbf_node(uint64_t node_id, double lat, double lon, uint8_t *coded_tags) {

    OSMPBF__Node *node = &(node_block[node_block_count]);
    osmpbf__node__init(node);
    node->id = node_id;
    node->lat = (int64_t)(lat * 10000000);
    node->lon = (int64_t)(lon * 10000000);

    size_t n_tags = load_tags(coded_tags, &(node->keys), &(node->vals));
    node->n_keys = n_tags;
    node->n_vals = n_tags;

    /* Write out a block if we've filled the buffer. */
    node_block_count++;
    if (node_block_count == PBF_BLOCK_SIZE) {
        write_pbf_data_blob(true, false);
    }

}




/* pbf.c */

#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <arpa/inet.h>
#include "zlib.h"

#define FILE "/var/otp/graphs/benelux/planet-benelux.osm.pbf"

// install protobuf-c-compiler and libprotobuf-c0-dev, then 
// protoc-c --c_out . ./osmformat.proto
// protoc-c --c_out . ./fileformat.proto

// "The uncompressed length of a Blob *should* be less than 16 MiB (16*1024*1024 bytes) 
// and *must* be less than 32 MiB."
#define MAX_BLOB_SIZE_UNCOMPRESSED 32 * 1024 * 1024

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

static void *map;
static size_t map_size;

static void pbf_map() {
    int fd = open(FILE, O_RDONLY);
    if (fd == -1) 
        die("could not find input file");
    struct stat st;
    if (stat(FILE, &st) == -1) 
        die("could not stat input file");
    map = mmap((void*)0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    map_size = st.st_size;
    if (map == (void*)(-1)) 
        die("could not map input file");
}

static void pbf_unmap() {
    munmap(map, map_size);
}

static unsigned char zbuf[MAX_BLOB_SIZE_UNCOMPRESSED];

static int zinflate(ProtobufCBinaryData *in, unsigned char *out) {
    int ret;
    
    /* initialize deflate state */
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        die("zlib init failed");

    /* ProtobufCBinaryData is {size_t len; uint8_t *data} */ 
    strm.avail_in = in->len;
    strm.next_in = in->data;

    strm.avail_out = MAX_BLOB_SIZE_UNCOMPRESSED;
    strm.next_out = out;

    ret = inflate(&strm, Z_NO_FLUSH);
    // check ret
    (void)inflateEnd(&strm);
    return MAX_BLOB_SIZE_UNCOMPRESSED - strm.avail_out;
    
}

static int noderefs = 0;
void handle_way(OSMPBF__Way *way) {
    for (int r = 0; r < way->n_refs; ++r) {
        ++noderefs;
    }
}

typedef struct osm_callbacks osm_callbacks_t;
struct osm_callbacks {
    void (*way) (OSMPBF__Way*);
    void (*node) (OSMPBF__Node*);
};

void handle_primitive_block(OSMPBF__PrimitiveBlock *block, osm_callbacks_t *callbacks) {
    size_t nstrings = block->stringtable->n_s;
    ProtobufCBinaryData *s = block->stringtable->s;
    int32_t granularity = block->has_granularity ? block->granularity : 1;
    int64_t lat_offset = block->has_lat_offset ? block->lat_offset : 0;
    int64_t lon_offset = block->has_lon_offset ? block->lon_offset : 0;
    //printf("pblock with granularity %d and offsets %d, %d\n", granularity, lat_offset, lon_offset);
    for (int g = 0; g < block->n_primitivegroup; ++g) { 
        OSMPBF__PrimitiveGroup *group = block->primitivegroup[g];
        //printf("pgroup with %d nodes, %d dense nodes, %d ways, %d relations\n",  group->n_nodes, 
        //    group->dense ? group->dense->n_id : 0, group->n_ways, group->n_relations);
        if (callbacks->way) {
            for (int w = 0; w < group->n_ways; ++w) {
                OSMPBF__Way *way = group->ways[w];
                (*(callbacks->way))(way);
            }
        }
        if (callbacks->node) {
            for (int n = 0; n < group->n_nodes; ++n) {
                OSMPBF__Node *node = group->nodes[n];
                (*(callbacks->node))(node);
            }
        }
    }
}

void scan_pbf(osm_callbacks_t *callbacks) {
    pbf_map();
    OSMPBF__HeaderBlock *header = NULL;
    int blobcount = 0;
    for (void *buf = map; buf < map + map_size; ++blobcount) {
        if (blobcount % 1000 == 0)
            printf("loading blob %d\n", blobcount);

        /* read blob header */
        OSMPBF__BlobHeader *blobh;
        // header prefixed with 4-byte contain network (big-endian) order message length
        int32_t msg_length = ntohl(*((int*)buf));
        buf += sizeof(int32_t);
        blobh = osmpbf__blob_header__unpack(NULL, msg_length, buf); 
        buf += msg_length;
        if (blobh == NULL)
            die("error unpacking blob header");
            
        /* read blob data */
        OSMPBF__Blob *blob;
        blob = osmpbf__blob__unpack(NULL, blobh->datasize, buf); 
        buf += blobh->datasize;
        if (blobh == NULL) 
            die("error unpacking blob data");
       
        /* check if the blob is raw or compressed */ 
        uint8_t* bdata;
        size_t bsize;        
        if (blob->has_zlib_data) {
            bdata = zbuf;
            bsize = blob->raw_size;
            int inflated_size = zinflate(&(blob->zlib_data), zbuf);
            if (inflated_size != bsize)
                die("inflated blob size does not match expected size");
        } else if (blob->has_raw) {
            printf("uncompressed\n");
            bdata = blob->raw.data;
            bsize = blob->raw.len;
        } else 
            die("neither compressed nor raw data present in blob");

        /* get header block from first blob */
        if (header == NULL) { 
            if (strcmp(blobh->type, "OSMHeader") != 0) 
                die("expected first blob to be a header");
            header = osmpbf__header_block__unpack(NULL, bsize, bdata); 
            if (header == NULL) 
                die("failed to read OSM header message from header blob");
            goto free_blob;
        } 

        /* get an OSM primitive block from subsequent blobs */
        if (strcmp(blobh->type, "OSMData") != 0) {
            printf("skipping unrecognized blob type\n");
            goto free_blob;
        }
        OSMPBF__PrimitiveBlock *block;
        block = osmpbf__primitive_block__unpack(NULL, bsize, bdata);
        if (block == NULL) 
            die("error unpacking primitive block");
        handle_primitive_block(block, callbacks);
        osmpbf__primitive_block__free_unpacked(block, NULL);
                
        /* post-iteration cleanup */
        free_blob: 
        osmpbf__blob_header__free_unpacked(blobh, NULL);
        osmpbf__blob__free_unpacked(blob, NULL);
    }    
    osmpbf__header_block__free_unpacked(header, NULL);
    pbf_unmap();
}

int main (int argc, const char * argv[]) {
    osm_callbacks_t callbacks;
    callbacks.way = &handle_way;
    callbacks.node = NULL;
    scan_pbf(&callbacks);
    printf("total node references %d\n", noderefs);
    return 0;
}



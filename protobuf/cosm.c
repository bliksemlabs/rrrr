#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <google/protobuf-c/protobuf-c.h> // contains varint functions
#include "intpack.h"
#include "pbf.h"
#include "tags.h"

// 14 bits -> 1.7km at 45 degrees
// 13 bits -> 3.4km at 45 degrees
// at 45 degrees cos(pi/4)~=0.7
// TODO maybe shift one more bit off of y to make more square
#define GRID_BITS 14
#define GRID_DIM (1 << GRID_BITS) /* The size of the grid root is 2^bits. */
#define MAX_NODE_ID 4000000000
#define MAX_WAY_ID 400000000
// ^ There are over 10 times as many nodes as ways in OSM

/* Way reference block size is based on the typical number of ways per grid cell. */
#define WAY_BLOCK_SIZE 32 
/* Assume half as many blocks as cells in the grid, considering grid is only ~15% full. */
#define MAX_WAY_BLOCKS (GRID_DIM * GRID_DIM / 2) 

/* The location where we will save all files. This can be set using a command line parameter. */
static const char *database_path = "/mnt/ssd2/cosm_db/";

/* Compact geographic position. Latitude and longitude mapped to the signed 32-bit int range. */
typedef struct {
    int32_t x;
    int32_t y; 
} coord_t;

/* Convert double-precision floating point latitude and longitude to internal representation. */
static void to_coord (/*OUT*/ coord_t *coord, double lat, double lon) {
    coord->x = (lon * INT32_MAX) / 180;
    coord->y = (lat * INT32_MAX) / 90;
} // TODO this is a candidate for return by value

/* Converts the y field of a coord to a floating point latitude. */
static double get_lat (coord_t *coord) {
    return ((double) coord->y) * 90 / INT32_MAX;
}

/* Converts the x field of a coord to a floating point longitude. */
static double get_lon (coord_t *coord) {
    return ((double) coord->x) * 180 / INT32_MAX;
}

/* A block of way references. Chained together to record which ways begin in each grid cell. */
typedef struct {
    int32_t refs[WAY_BLOCK_SIZE]; 
    uint32_t next; // index of next way block, or number of free slots if negative
} WayBlock;

/* 
  A single OSM node. An array of 2^64 these serves as a map from node ids to nodes. 
  OSM assigns node IDs sequentially, so you only need about the first 2^32 entries as of 2014.
  Note that when nodes are deleted their IDs are not reused, so there are holes in
  this range, but sparse file support in the filesystem should take care of that.
  "Deleted node ids must not be reused, unless a former node is now undeleted."
*/
typedef struct {
    coord_t coord; // compact internal representation of latitude and longitude
    uint32_t tags; // byte offset into the packed tags array where this node's tag list begins
} Node;

/* 
  A single OSM way. Like nodes, way IDs are assigned sequentially, so a zero-indexed array of these
  serves as a map from way IDs to ways.
*/
typedef struct {
    uint32_t node_ref_offset; // the index of the first node in this way's node list
    uint32_t tags; // byte offset into the packed tags array where this node's tag list begins
} Way;

/* 
  The spatial index grid. A node's grid bin is determined by right-shifting its coordinates.
  Initially this was a multi-level grid, but it turns out to work fine as a single level.
  Rather than being directly composed of way reference blocks, there is a level of indirection
  because the grid is mostly empty due to ocean and wilderness. TODO eliminate coastlines etc.
*/
typedef struct {
    uint32_t cells[GRID_DIM][GRID_DIM]; // contains indexes to way_blocks
} Grid;

/* Print human readable representation of a number of bytes into a static buffer. */
static char human_buffer[128];
char *human (size_t bytes) {
    /* Convert to a double, so division can yield results with decimal places. */
    double s = bytes; 
    if (s < 1024) {
        sprintf (human_buffer, "%.1lf bytes", s);
        return human_buffer;
    }
    s /= 1024;
    if (s < 1024) {
        sprintf (human_buffer, "%.1lf kB", s);
        return human_buffer;
    }
    s /= 1024;
    if (s < 1024) {
        sprintf (human_buffer, "%.1lf MB", s);
        return human_buffer;
    }
    s /= 1024;
    if (s < 1024) {
        sprintf (human_buffer, "%.1lf GB", s);
        return human_buffer;
    }
    s /= 1024;
    sprintf (human_buffer, "%.1lf TB", s);
    return human_buffer;
}

void die (char *s) {
    printf("%s\n", s);
    exit(EXIT_FAILURE);
}

/*
  Map a file in the database directory into memory, letting the OS handle paging.
  Note that we cannot reliably re-map a file to the same memory address, so the files should not 
  contain pointers. Instead we store array indexes, which can have the advantage of being 32-bits 
  wide. We map one file per OSM object type.
  
  Mmap will happily map a zero-length file to a nonzero-length block of memory, but a bus error 
  will occur when you try to write to the memory. 

  It is tricky to expand the mapped region on demand you'd need to trap the bus error.
  Instead we reserve enough address space for the maximum size we ever expect the file to reach. 
  Linux provides the mremap() system call for expanding or shrinking the size of a given mapping. 
  msync() flushes the changes in memory to disk.

  The ext3 and ext4 filesystems understand "holes" via the sparse files mechanism:
  http://en.wikipedia.org/wiki/Sparse_file#Sparse_files_in_Unix
  Creating 100GB of empty file by calling truncate() does not increase the disk usage. 
  The files appear to have their full size using 'ls', but 'du' reveals that no blocks are in use.
*/
void *map_file(const char *name, size_t size) {
    char buf[256];
    if (strlen(name) >= sizeof(buf) - strlen(database_path) - 2)
        die ("name too long");
    sprintf (buf, "%s/%s", database_path, name); 
    printf("mapping file '%s' of size %s\n", buf, human(size));
    // including O_TRUNC causes much slower write (swaps pages in?)
    int fd = open(buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    void *base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) 
        die("could not memory map file");
    if (ftruncate (fd, size - 1)) // resize file
        die ("error resizing file");
    return base;
}

/* Open a buffered read/write FILE in the database directory, performing some checks. */
FILE *open_file(const char *name) {
    char buf[256];
    if (strlen(name) >= sizeof(buf) - strlen(database_path) - 2)
        die ("name too long");
    sprintf (buf, "%s/%s", database_path, name); 
    printf("opening file '%s' as read/write stream\n", buf);
    FILE *file = fopen(buf, "w+");
    if (file == NULL) 
        die("could not open tag file for output");
    return file;
}

/* Arrays of memory-mapped structs. This is where we store the bulk of our data. */
Grid     *grid;
Node     *nodes;
Way      *ways;
WayBlock *way_blocks;
int64_t  *node_refs;  // A negative node_ref marks the end of a list of refs.
char     *tags;       // A coded & compressed stream of tags.
uint32_t n_node_refs; // The number of node refs currently used. 
// FIXME n_node_refs will eventually overflow. The fact that it's unsigned gives us a little slack.

/* A file stream for writing coded-compressed tags. TODO Maybe write to the mmaped file instead. */
FILE *tag_file;

/* 
  The number of way reference blocks currently allocated.
  Sparse files appear to be full of zeros until you write to them. Therefore we skip way block zero 
  so we can use the zero index to mean "no way block".
*/
uint32_t way_block_count = 1;
static uint32_t new_way_block() {
    if (way_block_count % 10000 == 0)
        printf("%dk way blocks in use out of %dk.\n", way_block_count/1000, MAX_WAY_BLOCKS/1000);
    if (way_block_count >= MAX_WAY_BLOCKS)
        die("More way reference blocks are used than expected.");
    // A negative value in the last ref entry gives the number of free slots in this block.
    way_blocks[way_block_count].refs[WAY_BLOCK_SIZE-1] = -WAY_BLOCK_SIZE; 
    // printf("created way block %d\n", way_block_count);
    return way_block_count++;
}

/* Get the x or y bin for the given x or y coordinate. */
static uint32_t bin (int32_t xy) {
    return ((uint32_t)(xy)) >> (32 - GRID_BITS); // unsigned: logical shift
}

/* Get the address of the grid cell for the given internal coord. */
static uint32_t *get_grid_cell(coord_t coord) {
    return &(grid->cells[bin(coord.x)][bin(coord.y)]);
}

/* 
  Given a Node struct, return the index of the way reference block at the head of the Node's grid
  cell, creating a new way reference block if the grid cell is currently empty. 
*/
static uint32_t get_grid_way_block (Node *node) {
    uint32_t *cell = get_grid_cell(node->coord);
    if (*cell == 0) {
        *cell = new_way_block();
    }
    // printf("xbin=%d ybin=%d\n", xb, yb);
    // printf("index=%d\n", index);
    return *cell;
}

/* TODO make this insert a new block instead of just setting the grid cell contents. */
static void set_grid_way_block (Node *node, uint32_t wbi) {
    uint32_t *cell = get_grid_cell(node->coord);
    *cell = wbi;
}

/* 
  Given parallel tag key and value arrays of length n containing string table indexes, 
  write compacted lists of key=value pairs to a file which do not require the string table. 
  Returns the byte offset of the beginning of the new tag list within that file. 
*/
static uint64_t write_tags (uint32_t *keys, uint32_t *vals, int n, ProtobufCBinaryData *string_table) {
    if (n == 0) return 0; // if there are no tags, always point to index 0, which should contain a single terminator char.
    uint64_t position = ftell(tag_file);
    for (int t = 0; t < n; t++) {
        ProtobufCBinaryData key = string_table[keys[t]];
        ProtobufCBinaryData val = string_table[vals[t]];
        // skip unneeded keys
        if (memcmp("created_by",  key.data, key.len) == 0 ||
            memcmp("import_uuid", key.data, key.len) == 0 ||
            memcmp("attribution", key.data, key.len) == 0 ||
            memcmp("source",      key.data, 6) == 0 ||
            memcmp("tiger:",      key.data, 6) == 0) {
            continue;
        }
        int8_t code = encode_tag(key, val);
        // Code always written out to encode a key and/or a value, or indicate they are free text.
        fputc(code, tag_file); 
        if (code == 0) { 
            // Code 0 means zero-terminated key and value are written out in full.
            fwrite(key.data, key.len, 1, tag_file);
            fputc(0, tag_file);
            fwrite(val.data, val.len, 1, tag_file);
            fputc(0, tag_file);
        } else if (code < 0) { 
            // Negative code provides key lookup, but value is written as zero-terminated free text.
            fwrite(val.data, val.len, 1, tag_file);
            fputc(0, tag_file);
        }
    }
    /* The tag list is terminated with a single character. TODO maybe use 0 as terminator. */
    fputc(INT8_MAX, tag_file); 
    return position;
}

/* Count the number of nodes and ways loaded, just for progress reporting. */
static long nodes_loaded = 0;
static long ways_loaded = 0;

/* Node callback handed to the general-purpose PBF loading code. */
static void handle_node (OSMPBF__Node *node, ProtobufCBinaryData *string_table) {
    if (node->id > MAX_NODE_ID) 
        die("OSM data contains nodes with larger IDs than expected.");
    if (ways_loaded > 0)
        die("All nodes must appear before any ways in input file.");
    double lat = node->lat * 0.0000001;
    double lon = node->lon * 0.0000001;
    to_coord(&(nodes[node->id].coord), lat, lon);
    nodes[node->id].tags = write_tags(node->keys, node->vals, node->n_keys, string_table);
    nodes_loaded++;
    if (nodes_loaded % 1000000 == 0)
        printf("loaded %ldM nodes\n", nodes_loaded / 1000 / 1000);
    //printf ("---\nlon=%.5f lat=%.5f\nx=%d y=%d\n", lon, lat, nodes[node->id].x, nodes[node->id].y); 
}

/* 
  Way callback handed to the general-purpose PBF loading code.
  All nodes must come before any ways in the input for this to work. 
*/
static void handle_way (OSMPBF__Way *way, ProtobufCBinaryData *string_table) {
    if (way->id > MAX_WAY_ID)
        die("OSM data contains ways greater IDs than expected.");
    /* 
       Copy node references into a sub-segment of one big array, reversing the PBF delta coding so 
       they are absolute IDs. All the refs within a way or relation are always known at once, so 
       we can use exact-length lists (unlike the lists of ways within a grid cell).
       Each way stores the index of the first node reference in its list, and a negative node 
       ID is used to signal the end of the list.
    */
    ways[way->id].node_ref_offset = n_node_refs;
    //printf("WAY %ld\n", way->id);
    //printf("node ref offset %d\n", ways[way->id].node_ref_offset);
    int64_t node_id = 0;
    for (int r = 0; r < way->n_refs; r++, n_node_refs++) {
        node_id += way->refs[r]; // node refs are delta coded
        node_refs[n_node_refs] = node_id;
        //printf("  NODE %ld\n", node_id);
    }
    node_refs[n_node_refs - 1] *= -1; // Negate last node ref to signal end of list.
    /* Index this way, as being in the grid cell of its first node. */
    uint32_t wbi = get_grid_way_block(&(nodes[way->refs[0]]));
    WayBlock *wb = &(way_blocks[wbi]);
    /* If the last node ref is non-negative, no free slots remain. Chain a new empty block. */
    if (wb->refs[WAY_BLOCK_SIZE - 1] >= 0) {
        int32_t n_wbi = new_way_block();
        // Insert new block at head of list to avoid later scanning though large swaths of memory.
        wb = &(way_blocks[n_wbi]);
        wb->next = wbi;
        set_grid_way_block(&(nodes[way->refs[0]]), n_wbi);
    }
    /* We are now certain to have a free slot in the current block. */
    int nfree = wb->refs[WAY_BLOCK_SIZE - 1];
    if (nfree >= 0) die ("Final ref should be negative, indicating number of empty slots.");
    /* A final ref < 0 gives the number of free slots in this block. */
    int free_idx = WAY_BLOCK_SIZE + nfree; 
    wb->refs[free_idx] = way->id;
    /* If this was not the last available slot, reduce number of free slots in this block by one. */
    if (nfree != -1) (wb->refs[WAY_BLOCK_SIZE - 1])++; 
    ways_loaded++;
    /* Save tags to compacted tag array, and record the index where that tag list begins. */
    ways[way->id].tags = write_tags(way->keys, way->vals, way->n_keys, string_table);
    if (ways_loaded % 100000 == 0) {
        printf("loaded %ldk ways\n", ways_loaded / 1000);
        printf("tag file at position %s\n", human(ways[way->id].tags));
    }
}

/* 
  Used for setting the grid side empirically.
  With 8 bit (255x255) grid, planet.pbf gives 36.87% full
  With 14 bit grid: 248351486 empty 20083970 used, 7.48% full
*/
static void fillFactor () {
    int used = 0;
    for (int i = 0; i < GRID_DIM; ++i) {
        for (int j = 0; j < GRID_DIM; ++j) {
            if (grid->cells[i][j] != 0) used++;
        }
    }
    printf("index grid: %d used, %.2f%% full\n", 
        used, ((double)used) / (GRID_DIM * GRID_DIM) * 100); 
}

/* Print out a message explaining command line parameters to the user, then exit. */
static void usage () {
    printf("usage:\ncosm database_dir input.osm.pbf\n");
    printf("cosm database_dir min_lat min_lon max_lat max_lon\n");
    exit(EXIT_SUCCESS);
}

/* Range checking. */
static void check_lat_range(double lat) {
    if (lat < -90 && lat > 90)
        die ("Latitude out of range.");
}

/* Range checking. */
static void check_lon_range(double lon) {
    if (lon < -180 && lon > 180)
        die ("Longitude out of range.");
}

/* Functions beginning with print_ output OSM in a simple structured text format. */
static void print_tags (uint32_t idx) {
    if (idx == UINT32_MAX) return; // special index indicating no tags
    char *t = &(tags[idx]);
    KeyVal kv;
    while (*t != INT8_MAX) {
        t += decode_tag(t, &kv);
        printf("%s=%s ", kv.key, kv.val);
    }
}

static void print_node (uint64_t node_id) {
    Node node = nodes[node_id];
    printf("  node %ld (%.6f, %.6f) ", node_id, get_lat(&node.coord), get_lon(&node.coord));
    print_tags(nodes[node_id].tags);
    printf("\n");
}

static void print_way (int64_t way_id) {
    printf("way %ld ", way_id);
    print_tags(ways[way_id].tags);
    printf("\n");
}

/* 
  Fields and functions for saving compact binary OSM. 
  This is comparable in size to PBF if you zlib it in blocks, but much simpler. 
*/
FILE *ofile;
int32_t last_x, last_y;
int64_t last_node_id, last_way_id;

static void save_init () {
    ofile = open_file("out.bin");
    last_x = 0;
    last_y = 0;
    last_node_id = 0;
    last_way_id = 0;
}

static void save_tags (uint32_t idx) {
    KeyVal kv;
    if (idx != UINT32_MAX) {
        char *t0 = &(tags[idx]);
        char *t = t0;
        while (*t != INT8_MAX) t += decode_tag(t, &kv);
        fwrite(t0, t - t0, 1, ofile);
    }
    fputc(INT8_MAX, ofile);
}

static void save_node (int64_t node_id) {
    Node node = nodes[node_id];
    uint8_t buf[10]; // 10 is max length of a 64 bit varint
    int64_t id_delta = node_id - last_node_id;
    int32_t x_delta = node.coord.x - last_x;
    int32_t y_delta = node.coord.y - last_y;
    size_t size;
    size = sint64_pack(id_delta, buf);
    fwrite(&buf, size, 1, ofile);
    size = sint32_pack(x_delta, buf);
    fwrite(&buf, size, 1, ofile);
    size = sint32_pack(y_delta, buf);
    fwrite(&buf, size, 1, ofile);
    save_tags(node.tags);
    last_node_id = node_id;
    last_x = node.coord.x;
    last_y = node.coord.y;
}

static void save_way (int64_t way_id) {
    Way way = ways[way_id];
    uint8_t buf[10]; // 10 is max length of a 64 bit varint
    int64_t id_delta = way_id - last_way_id;
    size_t size = sint64_pack(id_delta, buf);
    fwrite(&buf, size, 1, ofile);
    save_tags(way.tags);
    last_way_id = way_id;
}

int main (int argc, const char * argv[]) {

    if (argc != 3 && argc != 6) usage();
    database_path = argv[1];
    grid       = map_file("grid",       sizeof(Grid));
    nodes      = map_file("nodes",      sizeof(Node)     * MAX_NODE_ID);
    ways       = map_file("ways",       sizeof(Way)      * MAX_WAY_ID);
    node_refs  = map_file("node_refs",  sizeof(int64_t)  * MAX_NODE_ID);
    way_blocks = map_file("way_blocks", sizeof(WayBlock) * MAX_WAY_BLOCKS);
    tags       = map_file("tags",       UINT32_MAX);

    // ^ Assume that there are as many node refs as there are 
    // node ids including holes. [MAX_NODE_ID]
    if (argc == 3) {
        /* LOAD */
        const char *filename = argv[2];
        osm_callbacks_t callbacks;
        callbacks.way = &handle_way;
        callbacks.node = &handle_node;
        tag_file = open_file("tags");
        // all elements without tags will point to index 0, which contains the tag list terminator.
        fputc(INT8_MAX, tag_file); 
        scan_pbf(filename, &callbacks); // we could just pass the callbacks by value
        fclose(tag_file);
        fillFactor();
        printf("loaded %ld nodes and %ld ways total.\n", nodes_loaded, ways_loaded);
        return EXIT_SUCCESS;    
    } else if (argc == 6) {
        /* QUERY */
        double min_lat = strtod(argv[2], NULL);
        double min_lon = strtod(argv[3], NULL);
        double max_lat = strtod(argv[4], NULL);
        double max_lon = strtod(argv[5], NULL);
        printf("min = (%.5lf, %.5lf) max = (%.5lf, %.5lf)\n", min_lat, min_lon, max_lat, max_lon);
        check_lat_range(min_lat);
        check_lat_range(max_lat);
        check_lon_range(min_lon);
        check_lon_range(max_lon);
        if (min_lat >= max_lat) die ("min lat must be less than max lat.");
        if (min_lon >= max_lon) die ("min lon must be less than max lon.");
        coord_t cmin, cmax;
        to_coord(&cmin, min_lat, min_lon);
        to_coord(&cmax, max_lat, max_lon);
        uint32_t min_xbin = bin(cmin.x);
        uint32_t max_xbin = bin(cmax.x);
        uint32_t min_ybin = bin(cmin.y);
        uint32_t max_ybin = bin(cmax.y);
        FILE *pbf_file = open_file("out.pbf");
        write_pbf_begin(pbf_file);
        /* Make two passes, first outputting all nodes, then all ways. FIXME Intersection nodes are repeated. */
        for (int stage = 0; stage < 2; stage++) {
            for (uint32_t x = min_xbin; x <= max_xbin; x++) {
                for (uint32_t y = min_ybin; y <= max_ybin; y++) {
                    uint32_t wbidx = grid->cells[x][y];
                    // printf ("xbin=%d ybin=%d way bin index %u\n", x, y, wbidx);
                    if (wbidx == 0) continue; // There are no ways in this grid cell.
                    /* Iterate over all ways in this block, and its chained blocks. */
                    WayBlock *wb = &(way_blocks[wbidx]);
                    for (;;) {
                        for (int w = 0; w < WAY_BLOCK_SIZE; w++) {
                            int64_t way_id = wb->refs[w];
                            if (way_id <= 0) break;
                            Way way = ways[way_id];
                            if (stage == 1) {
                                write_pbf_way(way_id, &(node_refs[way.node_ref_offset]), 
                                    &(tags[way.tags]));
                            } else if (stage == 0) {
                                /* Output all nodes in this way. */
                                uint32_t nr = way.node_ref_offset;
                                bool more = true;
                                for (; more; nr++) {
                                    int64_t node_id = node_refs[nr];
                                    if (node_id < 0) {
                                        node_id = -node_id;
                                        more = false;
                                    }
                                    Node node = nodes[node_id];
                                    write_pbf_node(node_id, get_lat(&(node.coord)), 
                                        get_lon(&(node.coord)), (int8_t *) &(tags[node.tags]));
                                }
                            }
                        }
                        if (wb->next == 0) break;
                        wb = &(way_blocks[wb->next]);
                    }
                }
            }
            /* Write out any buffered nodes or ways before beginning the next PBF writing stage. */
            write_pbf_flush();
        }
        fclose(pbf_file);
    } 
    /* If neither load nor query mode was matched, tell the user how to call the program. */
    else usage();

}




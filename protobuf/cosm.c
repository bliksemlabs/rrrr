#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <google/protobuf-c/protobuf-c.h> // contains varint functions
#include "intpack.h"
#include "pbf.h"
#include "tags.h"

// Test this out on a small dataset.
// To test, can we just run this in memory and let swapping happen?
// Map long indexes to files of a fixed size (1 gigastruct), then index within file
// 14 bits -> 1.7km at 45 degrees
// 13 bits -> 3.4km at 45 degrees
// at 45 degrees cos(pi/4)~=0.7
// TODO maybe shift one more bit off of y to make more square
#define GRID_BITS 14
#define GRID_DIM (1 << GRID_BITS) /* The size of the grid root is 2^bits. */
#define MAX_NODE_ID 4000000000
#define MAX_WAY_ID 400000000
// ^ There are over 10 times as many nodes as ways in OSM
#define WAY_BLOCK_SIZE 32 /* Think of typical number of ways per grid cell... */
#define TAG_HIGHWAY_PRIMARY   1
#define TAG_HIGHWAY_SECONDARY 2
#define TAG_HIGHWAY_TERTIARY  3

static const char *database_path = "/mnt/ssd2/cosm_db/";

typedef struct {
    int32_t x;
    int32_t y; 
} coord_t;

static void to_coord (coord_t *coord, double lat, double lon) {
    coord->x = (lon * INT32_MAX) / 180;
    coord->y = (lat * INT32_MAX) / 90;
}

static double get_lat (coord_t *coord) {
    return ((double) coord->y) * 90 / INT32_MAX;
}

static double get_lon (coord_t *coord) {
    return ((double) coord->x) * 180 / INT32_MAX;
}

typedef struct {
    int32_t refs[WAY_BLOCK_SIZE]; 
    uint32_t next; // index of next way block, or number of free slots if negative
} WayBlock; // actually way /references/

/* 
  A node's int32 coordinates, from which we can easily determine its grid bin by bitshifting. 
  An array of 2^64 these serves as a map from node ids to nodes. Node IDs are assigned
  sequentially, so you really need less than 2^32 entries as of 2014.
  Note that when nodes are deleted their IDs are not reused, so there are holes in
  this range, but the filesystem should take care of that.
*/
typedef struct {
    coord_t coord;
    uint32_t tags; // offset into the tags file
} Node;

typedef struct {
    uint32_t node_ref_offset;
    uint32_t tags; // offset into the tags file
} Way;

/* 
  The leaves of the grid tree. 
  These contain linked lists of nodes and ways. 
  Assuming each cell contains at least one node and one way, we could avoid a level 
  of indirection by storing the head nodes of the lists directly here rather then indexes 
  to dynamically allocated ones, with additional blocks being dynamically allocated.
  However this makes for a really big file, and some grid cells will in fact be empty,
  so we can use UINT32_MAX to mean "none". It is already 11GB with only the int indexes here.
*/

/* 
  The intermediate level of the grid tree. 
  These nodes consume the most significant remaining 8 bits on each axis.
  There can be at most (2^16 * 2^16) == 2^32 leaf nodes, so we could use 32 bit integers to 
  index them. However, unlike the root node, there is probably some object in every subgrid. 
  If that is the case then there is no need for any indirection, we can just store the bin 
  structs directly at this level.
*/

/* 
  The spatial index grid.
  It is likely to be only 1/3 full due to ocean and wilderness. 
  TODO eliminate coastlines etc.
*/
typedef struct {
    uint32_t cells[GRID_DIM][GRID_DIM]; // contains indexes to way_blocks
} Grid;

// cannot mmap to the same location, so don't use pointers. 
// instead use object indexes (which could be 32-bit!) and one mmapped file per object type.
// sort nodes so we can do a binary search?
// " Deleted node ids must not be reused, unless a former node is now undeleted. "

/* Print human readable size into a static buffer. */
static char human_buffer[128];
char *human (double s) {
    // parameter is a double, so division can yield results with some decimal places
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
  mmap will map a zero-length file, in which case it will cause a bus error when it tries to write. 
  you could "reserve" the address space for the maximum size of the file by 
  providing that size when the file is first mapped in with mmap().
  Linux provides the mremap() system call for expanding or shrinking the size of a given mapping. 
  msync() flushes.
  The ext3 and ext4 filesystems understand "holes" via the sparse files mechanism:
  http://en.wikipedia.org/wiki/Sparse_file#Sparse_files_in_Unix
  Creating 100GB of empty file by calling truncate() does not increase the disk usage, 
  though the files appear to have their full size in 'ls' for example.
  So you can make the file as big as you'll ever need, and the filesystem will take care of it.
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

/* Open a file in the database directory, performing some checks. */
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

/* Arrays of memory-mapped structs */
Grid     *grid;
Node     *nodes;
Way      *ways;
WayBlock *way_blocks;
int64_t  *node_refs;  // negative means last in a list of refs.
uint32_t n_node_refs; // the number of used node refs. FIXME is this going to overflow? ~2x current number of nodes in planet...
char     *tags;       // a coded & compressed stream of tags

/* A file where tags are being written. Maybe replace with mmaped file. */
FILE *tag_file;

/* 
  The number way blocks currently allocated.
  Start at 1 so we can use the "empty file" 0 value to mean "unassigned". 
*/
uint32_t way_block_count  = 1;
static uint32_t new_way_block() {
    if (way_block_count % 10000 == 0)
        printf("%dk way blocks in use.\n", way_block_count / 1000);
    // neg value in last ref -> number of free slots
    way_blocks[way_block_count].refs[WAY_BLOCK_SIZE-1] = -WAY_BLOCK_SIZE; 
    // printf("created way block %d\n", way_block_count);
    return way_block_count++;
}

// note that this is essentially a node stem plus offsets. 
// we could either explicitly use the node stem type, or save the whole mess in the node table
// with only the node references in the grid leaves. This also avoids having to scan through the bin
// to remove a node.
// then the node and way lists could just be copy-on-write compacted reference lists.
// or to begin with, uncompacted reference lists.
// this could in fact be a union type of two 32-bit x and y or four 16-bit bins or 
// two 8-bit bin levels and one 16-bit offset per axis.
// 8block-8leaf-16offset

// replace internal_coord_t with a union

static uint32_t bin (int32_t coord) {
    return ((uint32_t)(coord)) >> (32 - GRID_BITS); // unsigned: logical shift
}

static uint32_t *get_grid_cell(coord_t coord) {
    return &(grid->cells[bin(coord.x)][bin(coord.y)]);
}

static uint32_t get_grid_way_block (Node *node) {
    uint32_t *cell = get_grid_cell(node->coord);
    if (*cell == 0) {
        *cell = new_way_block();
    }
    // printf("xbin=%d ybin=%d\n", xb, yb);
    // printf("index=%d\n", index);
    return *cell;
}

// TODO make this insert a new block instead of just setting it
static void set_grid_way_block (Node *node, uint32_t wbi) {
    uint32_t *cell = get_grid_cell(node->coord);
    *cell = wbi;
}

/* Given parallel arrays of length n containing key and value string indexes, write k=v to a file. */
static uint32_t write_tags (uint32_t *keys, uint32_t *vals, int n, ProtobufCBinaryData *string_table) {
    if (n == 0) return UINT32_MAX;
    uint64_t position = ftell(tag_file);
    if (position % (1024 * 1024 * 100) == 0 && position != 0) {
        printf("tag file at position %s\n", human(position));
    }
    for (int t = 0; t < n; t++) {
        ProtobufCBinaryData key = string_table[keys[t]];
        ProtobufCBinaryData val = string_table[vals[t]];
        // skip unneeded keys
        if (memcmp("source",      key.data, key.len) == 0 ||
            memcmp("created_by",  key.data, key.len) == 0 ||
            memcmp("import_uuid", key.data, key.len) == 0 ||
            memcmp("attribution", key.data, key.len) == 0 ||
            memcmp("tiger:",      key.data, 6) == 0) {
            continue;
        }
        int8_t code = encode_tag(key, val);
        fputc(code, tag_file); // code is always written to show type of kv storage
        if (code == 0) { // key and value are written out in full
            fwrite(key.data, key.len, 1, tag_file);
            fputc(0, tag_file);
            fwrite(val.data, val.len, 1, tag_file);
            fputc(0, tag_file);
        } else if (code < 0) { // value is written as zero-terminated free text
            fwrite(val.data, val.len, 1, tag_file);
            fputc(0, tag_file);
        }
    }
    fputc(INT8_MAX, tag_file); // terminate tag list
    return position;
}

static long nodes_loaded = 0;
static long ways_loaded = 0;

// Considering ocean covers 70 percent of the earth and the total number of theoretical 
// grid bins is 2^16, we would estimate (UINT16_MAX / 3) ~= 22000 blocks actually used.
// Experiment gives a similar but somewhat higher number of ~24000 grid blocks actually used 
// when loading the entire planet.pbf.
// #define MAX_GRID_BLOCKS (30000)
// Max number of node/way blocks. This is probably an underestimate due to underfilled node blocks.
// The ratio of node to way block sizes should make this value also usable for ways.
#define MAX_WAY_BLOCKS (0.5 * GRID_DIM * GRID_DIM) /* about 100 Mstructs */
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

#define REF_HISTOGRAM_BINS 20
static int node_ref_histogram[REF_HISTOGRAM_BINS]; // initialized and displayed in main

/* Nodes must come before ways for this to work. */
static void handle_way (OSMPBF__Way *way, ProtobufCBinaryData *string_table) {
    if (way->id > MAX_WAY_ID)
        die("OSM data contains ways greater IDs than expected.");
    int nr = (way->n_refs > REF_HISTOGRAM_BINS - 1) ? REF_HISTOGRAM_BINS - 1 : way->n_refs;
    node_ref_histogram[nr]++;
    // Copy node references into. 
    // NOTE that all refs in a list are always known at once, so we can use exact-length lists. 
    // Each way stores the position of its first node ref, and a negative ref is used
    // to signal the end of the list.
    ways[way->id].node_ref_offset = n_node_refs;
    int64_t node_id = 0;
    for (int r = 0; r < way->n_refs; r++, n_node_refs++) {
        node_id += way->refs[r]; // node refs are delta coded
        node_refs[n_node_refs] = node_id;
    }
    node_refs[n_node_refs - 1] *= -1; // Negate last node ref to signal end of list.
    // Index this way, as being in the grid cell of its first node
    uint32_t wbi = get_grid_way_block(&(nodes[way->refs[0]]));
    WayBlock *wb = &(way_blocks[wbi]);
    // last ref >= 0 means no free slots remaining. chain an empty block.
    // insert at head to avoid too much scanning though large swaths of memory.
    if (wb->refs[WAY_BLOCK_SIZE - 1] >= 0) {
        int32_t n_wbi = new_way_block();
        wb = &(way_blocks[n_wbi]);
        wb->next = wbi;
        set_grid_way_block(&(nodes[way->refs[0]]), n_wbi);
    }
    // We are now certain to have a free slot in the current block.
    int nfree = wb->refs[WAY_BLOCK_SIZE - 1];
    if (nfree >= 0) die ("failed assertion: final ref should be negative.");
    // a final ref < 0 gives the number of remaining slots.
    int free_idx = WAY_BLOCK_SIZE + nfree; 
    wb->refs[free_idx] = way->id;
    // If this was not the last available slot, reduce number of free slots in this block by one
    if (nfree != -1) (wb->refs[WAY_BLOCK_SIZE - 1])++; 
    ways_loaded++;
    ways[way->id].tags = write_tags(way->keys, way->vals, way->n_keys, string_table);
    if (ways_loaded % 100000 == 0)
        printf("loaded %ldk ways\n", ways_loaded / 1000);
}

/* 
  With 8 bit (255x255) grid, planet.pbf gives 36.87% full
  With 14 bit grid: 248351486 empty 20083970 used, 7.48% full (!)
  With 13 bit grid: 248351486 empty 20083970 used, 10.76% full
*/
void fillFactor () {
    int used = 0;
    for (int i = 0; i < GRID_DIM; ++i) {
        for (int j = 0; j < GRID_DIM; ++j) {
            if (grid->cells[i][j] != 0) used++;
        }
    }
    printf("index grid: %d used, %.2f%% full\n", 
        used, ((double)used) / (GRID_DIM * GRID_DIM) * 100); 
}

static void usage () {
    printf("usage:\ncosm database_dir input.osm.pbf\n");
    printf("cosm database_dir min_lat min_lon max_lat max_lon\n");
    exit(EXIT_SUCCESS);
}

static void check_lat_range(double lat) {
    if (lat < -90 && lat > 90)
        die ("latitude out of range.");
}

static void check_lon_range(double lon) {
    if (lon < -180 && lon > 180)
        die ("longitude out of range.");
}

static void print_tags (uint32_t idx) {
    if (idx == UINT32_MAX) return; // special index indicating no tags
    char *t = &(tags[idx]);
    KeyVal kv;
    while (*t != INT8_MAX) {
        t += decode_tag(t, &kv);
        printf("%s=%s ", kv.key, kv.val);
    }
}

// For saving compact OSM
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

/* Reusable buffers for loading internal ways into OSMPBF ways. */
#define MAX_REFS 2048
static int64_t refs_buf[MAX_REFS];

/* Load a COSM Way from the internal representation into an OSMPBF__Way struct. */
static void load_pbf_way(OSMPBF__Way *pbf_way, uint64_t way_id) {
    Way way = ways[way_id];
    uint32_t nri = way.node_ref_offset;
    size_t n_refs = 0;
    // We can't just point to the refs mmap since A) it contains negative values and B) we need to delta code.
    int64_t prev_ref = 0; 
    for (;;) {
        int64_t ref = node_refs[nri++];
        if (ref < 0) {
            refs_buf[n_refs++] = (-ref) - prev_ref; 
            break;
        }
        refs_buf[n_refs++] = ref - prev_ref; // refs within a way are delta coded
        if (n_refs == MAX_REFS) {
            printf("Hit maximum number of references in a way, skipping some.\n");
            break;
        }
        prev_ref = ref;
    }
/*
    prev_ref = 0;
    for (int i=0; i<n_refs; i++) {
        int64_t ref = refs_buf[i];
        ref += prev_ref;
        printf("%d %ld %ld\n", i, refs_buf[i], ref);
        prev_ref = ref;
        if (ref < 0) exit(-1);
    }
*/
    osmpbf__way__init(pbf_way);
    pbf_way->id = way_id;
    pbf_way->refs = refs_buf;
    pbf_way->n_refs = n_refs;
    /* LOAD_TAGS
    way.n_keys;
    way.keys;
    way.n_vals;
    way.vals;
    */
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
        for (int i=0; i<REF_HISTOGRAM_BINS; i++) 
            node_ref_histogram[i] = 0;
        tag_file = open_file("tags");
        scan_pbf(filename, &callbacks); // we could just pass the callbacks by value
        fclose(tag_file);
        fillFactor();
        for (int i=0; i<REF_HISTOGRAM_BINS; i++) 
            printf("%2d refs: %d\n", i, node_ref_histogram[i]);
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
        OSMPBF__Way pbf_way;

        save_init();
        for (uint32_t x = min_xbin; x <= max_xbin; x++) {
            for (uint32_t y = min_ybin; y <= max_ybin; y++) {
                uint32_t wbidx = grid->cells[x][y];
                // printf ("xbin=%d ybin=%d way bin index %u\n", x, y, wbidx);
                if (wbidx == 0) continue; // no ways in this cell
                WayBlock *wb = &(way_blocks[wbidx]);
                for (;;) {
                    for (int w = 0; w < WAY_BLOCK_SIZE; w++) {
                        int32_t way_id = wb->refs[w];
                        if (way_id <= 0) break;
                        save_way(way_id);

                        load_pbf_way(&pbf_way, way_id);
                        write_pbf_way(&pbf_way);

                        int32_t nr = ways[way_id].node_ref_offset;
                        for (;; nr++) {
                            int64_t node_id = node_refs[nr];
                            if (node_id < 0) {
                                node_id = -node_id;
                                save_node(node_id);

                                Node node = nodes[node_id];
                                write_pbf_node(node_id, get_lat(&(node.coord)), get_lon(&(node.coord)), (int8_t*) &(tags[node.tags]));

                                //printf("\n");
                                break;
                            }
                            save_node(node_id);
                        }
                    }
                    if (wb->next == 0) break;
                    wb = &(way_blocks[wb->next]);
                }
            }
        }
        
        write_pbf_flush();
        fclose(pbf_file);
        
    } else usage();

}




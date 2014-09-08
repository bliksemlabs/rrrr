#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include "/home/abyrd/git/rrrr/protobuf/pbf.h"

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
#define WAY_BLOCK_SIZE 32 /* Per grid cell */
#define NODE_BLOCK_SIZE 10
#define TAG_HIGHWAY_PRIMARY   1
#define TAG_HIGHWAY_SECONDARY 2
#define TAG_HIGHWAY_TERTIARY  3

static const char *database_path = "/mnt/ssd2/cosm_db/";

typedef struct {
    int64_t refs[NODE_BLOCK_SIZE]; 
} NodeBlock; // actually node /references/

typedef struct {
    int32_t refs[WAY_BLOCK_SIZE]; 
    int32_t next; // index of next way block, or number of free slots if negative
} WayBlock; // actually way /references/

/* 
  A node's int32 coordinates, from which we can easily determine its grid bin. 
  An array of 2^64 these serves as a map from node ids to nodes. Node IDs are assigned
  sequentially, so really you only need about 2^33 entries as of 2014.
  Note that when nodes are deleted their IDs are not reused, so there are holes in
  this range, but the filesystem should take care of that.
*/
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t tags; // bit flags for common tags we care about. highway = ?
} Node;

typedef struct {
    NodeBlock node_refs; // the head block of node refs is included directly in the Way
    uint32_t tags; // bit flags for common tags we care about. highway = ?
} Way;

/* 
  The leaves of the grid tree. 
  These contain linked lists of nodes and ways. 
  Assuming each cell contains at least one node and one way, we could avoid a level 
  of indirection by storing the head nodes of the lists directly here rather then indexes 
  to dynamically allocated ones, with additional blocks being dynamically allocated.
  However this makes for a really big file, and some grid cells will in fact be empty,
  so we can use UINT32_MAX to mean "none". It is already 11GB with only the int indexes here.
typedef struct {
    uint32_t nodes;
    uint32_t ways;
} GridLeaf;
*/

/* 
  The intermediate level of the grid tree. 
  These nodes consume the most significant remaining 8 bits on each axis.
  There can be at most (2^16 * 2^16) == 2^32 leaf nodes, so we could use 32 bit integers to 
  index them. However, unlike the root node, there is probably some object in every subgrid. 
  If that is the case then there is no need for any indirection, we can just store the bin 
  structs directly at this level.
typedef struct {
    GridLeaf leaves[256][256];
} GridBlock;
*/

/* 
  The root of the grid tree. It consumes the most significant 8 bits of each axis.
  It is likely to be only 1/3 full due to ocean and wilderness. 
  Note that it can have at most 65536=2^16 children, one for each cell. 
  Therefore, they can be indexed with 16 bit integers.
  TODO it would be reasonable to double the resolution... but then we'd need 32bit int indexes.
  TODO check fill rate (do little sea islands make a difference)?
  TODO eliminate coastlines etc.
*/
typedef struct {
    int32_t blocks[GRID_DIM][GRID_DIM]; // contains indexes to way_blocks
} GridRoot;

// cannot mmap to the same location, so don't use pointers. 
// instead use object indexes (which could be 32-bit!) and one mmapped file per object type.
// sort nodes so we can do a binary search?
// " Deleted node ids must not be reused, unless a former node is now undeleted. "

/* Print human readable size. */
void human (double s) {
    if (s < 1024) {
        printf ("%.1lfbytes", s);
        return;
    }
    s /= 1024;
    if (s < 1024) {
        printf ("%.1lfkB", s);
        return;
    }
    s /= 1024;
    if (s < 1024) {
        printf ("%.1lfMB", s);
        return;
    }
    s /= 1024;
    if (s < 1024) {
        printf ("%.1lfGB", s);
        return;
    }
    s /= 1024;
    printf ("%.1lfTB", s);
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
  Interestingly the ext4 filesystem seems to understand "holes". Creating 100GB of empty file by
  calling truncate() does not increase the disk usage, though the files appear to have their full size.
  So you can make the file as big as you'll ever need, and the filesystem will take care of it! 
*/
void *map_file(const char *name, size_t size) {
    char buf[256];
    if (strlen(name) >= sizeof(buf) - strlen(database_path) - 2)
        die ("name too long");
    sprintf (buf, "%s/%s", database_path, name); 
    printf("mapping file '%s' of size ", buf);
    human(size);
    printf("\n");
    // including O_TRUNC causes much slower write (swaps pages in?)
    int fd = open(buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    void *base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) 
        die("could not memory map file");
    if (ftruncate (fd, size - 1)) // resize file
        die ("error resizing file");
    return base;
}

/* Arrays of memory-mapped structs */
GridRoot  *gridRoot;
Node      *nodes;
Way       *ways;
NodeBlock *node_blocks;
WayBlock  *way_blocks;

/* 
  The number of grid, node, and way blocks currently allocated.
  Start at 1 so we can use the "empty file" 0 value to mean "unassigned". 
*/ 
uint32_t way_block_count  = 1;
uint32_t node_block_count = 1;

// Maybe use mmap default file contents value of 0?

static int32_t new_node_block() {
    if (node_block_count % 100000 == 0)
        printf("%dk node ref blocks in use.\n", node_block_count / 1000);
    // node ref blocks are memory mapped, behave as if prefilled with zeros
    return node_block_count++;
}

static uint32_t new_way_block() {
    if (way_block_count % 100000 == 0)
        printf("%dk way blocks in use.\n", way_block_count / 1000);
    way_blocks[way_block_count].next = -WAY_BLOCK_SIZE; // neg -> number of free slots
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

static uint32_t get_grid_way_block (Node *node) {
    uint32_t xb = ((uint32_t)node->x) >> (32 - GRID_BITS); // unsigned: logical shift
    uint32_t yb = ((uint32_t)node->y) >> (32 - GRID_BITS); // unsigned: logical shift
    uint32_t index = gridRoot->blocks[xb][yb];
    if (index == 0) {
        index = new_way_block();
        gridRoot->blocks[xb][yb] = index;
    }
    // printf("xbin=%d ybin=%d\n", xb, yb);
    // printf("index=%d\n", index);
    return index;
}

static int32_t lon_to_x (double lon) {
    return (lon * INT32_MAX) / 180;
}

static int32_t lat_to_y (double lat) {
    return (lat * INT32_MAX) / 90;
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
#define MAX_WAY_BLOCKS (0.35 * GRID_DIM * GRID_DIM) /* about 100 Mstructs */
#define MAX_NODE_BLOCKS MAX_WAY_ID /* One additional block of node refs per way. */

static void handle_node (OSMPBF__Node *node) {
    if (node->id > MAX_NODE_ID) 
        die("OSM data contains nodes with larger IDs than expected.");
    if (ways_loaded > 0)
        die("All nodes must appear before any ways in input file.");
    double lat = node->lat * 0.0000001;
    double lon = node->lon * 0.0000001;
    nodes[node->id].x = lon_to_x(lon);
    nodes[node->id].y = lat_to_y(lat);
    nodes_loaded++;
    if (nodes_loaded % 100000000 == 0)
        printf("loaded %ldM nodes\n", nodes_loaded / 1000 / 1000);
    //printf ("---\nlon=%.5f lat=%.5f\nx=%d y=%d\n", lon, lat, nodes[node->id].x, nodes[node->id].y); 
}

#define REF_HISTOGRAM_BINS 20
static int node_ref_histogram[REF_HISTOGRAM_BINS]; // initialized and displayed in main

/* Nodes must come before ways for this to work. */
static void handle_way (OSMPBF__Way *way) {
    if (way->id > MAX_WAY_ID)
        die("OSM data contains ways greater IDs than expected.");
    int nr = (way->n_refs > REF_HISTOGRAM_BINS - 1) ? REF_HISTOGRAM_BINS - 1 : way->n_refs;
    node_ref_histogram[nr]++;
    NodeBlock *nb = &(ways[way->id].node_refs);
    int idx = 0; // index within the node block
    for (int r = 0; r < way->n_refs; r++) {
        int64_t node_id = way->refs[r];
        if (node_id > MAX_NODE_ID)
            die("Way references node with larger ID than expected.");
        // If we are going to overflow this block, chain another one
        if (idx == (NODE_BLOCK_SIZE - 1) && (way->n_refs - r > 1)) { 
            int32_t n_nb = new_node_block();
            nb->refs[idx] = -n_nb; // negative ref means block continues at this index
            nb = &(node_blocks[n_nb]);
            idx = 0;
        }
        nb->refs[idx] = node_id;
        idx++;
    }
    // Index this way in the grid cell of its first node
    uint32_t wbi = get_grid_way_block(&(nodes[way->refs[0]]));
    WayBlock *wb = &(way_blocks[wbi]);
    // next > 0 is a chained way block index. Skip forward to last block. TODO insert at head?
    while (wb->next > 0) wb = &(way_blocks[wb->next]);
    if (wb->next == 0) {
        // next == 0 means no free slots remaining. chain another block.
        int32_t wbi = new_way_block();
        wb->next = wbi;
        wb = &(way_blocks[wbi]);
    }
    // We are now certain to have a free slot in the current block.
    if (wb->next >= 0) die ("failed assertion: next should be negative.");
    // next < 0 gives the number of remaining slots.
    int free_idx = WAY_BLOCK_SIZE + wb->next; 
    wb->refs[free_idx] = way->id;
    (wb->next)++; // reduce number of free slots in this block by one
    ways_loaded++;
    if (ways_loaded % 10000000 == 0)
        printf("loaded %ldM ways\n", ways_loaded / 1024 / 1024);
}

/*
    if (leaf->nodes == 0) 
        leaf->nodes = new_node_block();
    NodeBlock *nblock = &(node_blocks[leaf->nodes]);
    // Fast-forward to end of linked list (actually we should insert new blocks at the head)
    while (nblock->next != NEXT_NONE)
        nblock = &(node_blocks[nblock->next]);
    // Insert a new block at the head if this block is full.
    if (nblock->nodes[NODE_BLOCK_SIZE - 1] != NODE_ID_UNUSED) {
        uint32_t nbidx = new_node_block();
        nblock = &(node_blocks[nbidx]);
        nblock->next = leaf->nodes;
        leaf->nodes = nbidx;
    }
    for (int n = 0; n < NODE_BLOCK_SIZE; ++n) {
        if (nblock->nodes[n] == NODE_ID_UNUSED) {
            nblock->nodes[n] = node->id;
            //printf("inserted %d,%d:%d\n", coord.xbin, coord.ybin, n); 
            break;
        }
    }
*/

/* 
  With 8 bit (255x255) grid, planet.pbf gives 36.87% full
  With 14 bit grid: 248351486 empty 20083970 used, 7.48% full (!)
  With 13 bit grid: 248351486 empty 20083970 used, 10.76% full
*/
void fillFactor () {
    int used = 0;
    for (int i = 0; i < GRID_DIM; ++i) {
        for (int j = 0; j < GRID_DIM; ++j) {
            if (gridRoot->blocks[i][j] != 0) used++;
        }
    }
    printf("index grid: %d used, %.2f%% full\n", 
        used, ((double)used) / (GRID_DIM * GRID_DIM) * 100); 
}

int main (int argc, const char * argv[]) {

    if (argc < 2) die("usage: cosm input.osm.pbf [database_dir]");
    const char *filename = argv[1];
    if (argc > 2) database_path = argv[2];
    gridRoot    = map_file("grid_root",   sizeof(GridRoot));
    nodes       = map_file("nodes",       sizeof(Node)      * MAX_NODE_ID);
    ways        = map_file("ways",        sizeof(Way)       * MAX_WAY_ID);
    node_blocks = map_file("node_blocks", sizeof(NodeBlock) * MAX_NODE_BLOCKS);
    way_blocks  = map_file("way_blocks",  sizeof(WayBlock)  * MAX_WAY_BLOCKS);

    osm_callbacks_t callbacks;
    callbacks.way = &handle_way;
    callbacks.node = &handle_node;
    fillFactor();
    for (int i=0; i<REF_HISTOGRAM_BINS; i++) node_ref_histogram[i] = 0;
    scan_pbf(filename, &callbacks);
    fillFactor();
    for (int i=0; i<REF_HISTOGRAM_BINS; i++) printf("%2d refs: %d\n", i, node_ref_histogram[i]);
    printf("loaded %ld nodes and %ld ways total.\n", nodes_loaded, ways_loaded);
    return EXIT_SUCCESS;

// TODO check fill proportion of grid blocks

}



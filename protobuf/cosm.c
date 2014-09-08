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

#define MAX_NODE_ID 4000000000
#define WAY_BLOCK_SIZE 16
#define NODE_BLOCK_SIZE 64
// ^ There are over 10 times as many nodes as ways in OSM
#define TAG_HIGHWAY_PRIMARY   1
#define TAG_HIGHWAY_SECONDARY 2
#define TAG_HIGHWAY_TERTIARY  3

static const char *database_path = "/mnt/ssd2/cosm_db/";

/* 
  The most significant half of a node's coordinates, which determines its bin. 
  An array of 2^64 these serves as a map from node ids to bins. Node IDs are assigned
  sequentially, so really you only need about 2^33 entries as of 2014.
*/
typedef struct {
    uint16_t xbin;
    uint16_t ybin;
    // int32_t index (the index within the block... for now just do a linear search within bin) 
} NodeStem;

typedef struct {
    int64_t id; // 2^64 possible nodes. id == 0 means unused.
    uint16_t x; // least significant 16 bits of lon
    uint16_t y; // least significant 16 bits of lat
    uint32_t tags; // bit flags for common tags we care about. highway = ?
} Node;

typedef struct {
    int32_t id; // 2^32 possible ways. id == 0 means unused.
    uint32_t tags; // bit flags for common tags we care about. highway = ?
} Way;

typedef struct {
    //Node nodes[NODE_BLOCK_SIZE];
    int64_t nodes[NODE_BLOCK_SIZE];
    uint32_t next; // int indexes into mmapped arrays, not pointers (keep relocatable)
} NodeBlock;

typedef struct {
    Way ways[WAY_BLOCK_SIZE];
    uint32_t next;
} WayBlock;

/* 
  The leaves of the grid tree. 
  These contain linked lists of nodes and ways. 
  Assuming each cell contains at least one node and one way, we could avoid a level 
  of indirection by storing the head nodes of the lists directly here rather then indexes 
  to dynamically allocated ones, with additional blocks being dynamically allocated.
  However this makes for a really big file, and some grid cells will in fact be empty,
  so we can use UINT32_MAX to mean "none". It is already 11GB with only the int indexes here.
*/
typedef struct {
    uint32_t nodes;
    uint32_t ways;
} GridLeaf;

/* 
  The intermediate level of the grid tree. 
  These nodes consume the most significant remaining 8 bits on each axis.
  There can be at most (2^16 * 2^16) == 2^32 leaf nodes, so we could use 32 bit integers to 
  index them. However, unlike the root node, there is probably some object in every subgrid. 
  If that is the case then there is no need for any indirection, we can just store the bin 
  structs directly at this level.
*/
typedef struct {
    GridLeaf leaves[256][256];
} GridBlock;

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
    uint16_t blocks[256][256];
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
GridBlock *gridBlocks;
NodeStem  *nodeStems;
NodeBlock *nodeBlocks;
WayBlock  *wayBlocks;

/* 
  The number of grid, node, and way blocks currently allocated.
  Start at 1 so we can use the "empty file" 0 value to mean "unassigned". 
*/ 
uint16_t grid_block_count  = 1; 
uint32_t way_block_count  = 1;
uint32_t node_block_count = 1;

// Maybe use mmap default file contents value of 0?
#define NEXT_NONE UINT32_MAX
#define NODE_ID_UNUSED INT64_MIN
#define WAY_ID_UNUSED INT32_MIN

static uint16_t new_grid_block() {
    if (grid_block_count % 1000 == 0)
        printf("%dk grid blocks in use.\n", grid_block_count / 1000);
    return grid_block_count++;
}

static uint32_t new_node_block() {
    if (node_block_count % 1000000 == 0)
        printf("%dM node blocks in use.\n", node_block_count / 1000000);
    for (int n = 0; n < NODE_BLOCK_SIZE; n++)
        nodeBlocks[node_block_count].nodes[n] = NODE_ID_UNUSED;
    nodeBlocks[node_block_count].next = NEXT_NONE;
    return node_block_count++;
}

static uint32_t new_way_block() {
    if (way_block_count % 1000 == 0)
        printf("%d way blocks in use.\n", way_block_count);
    for (int w = 0; w < WAY_BLOCK_SIZE; w++)
        wayBlocks[way_block_count].ways[w].id = WAY_ID_UNUSED;
    wayBlocks[way_block_count].next = NEXT_NONE;
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
typedef struct {
    uint16_t xbin;
    uint16_t ybin;
    uint16_t xoff;
    uint16_t yoff;
} internal_coord_t; 

static GridLeaf *get_grid_leaf (internal_coord_t *coord) {
    uint8_t x0 = coord->xbin >> 8;
    uint8_t y0 = coord->ybin >> 8;
    uint8_t x1 = coord->xbin & 0xFF;
    uint8_t y1 = coord->ybin & 0xFF;
    uint16_t index = gridRoot->blocks[x0][y0];
    if (index == 0) {
        index = new_grid_block();
        gridRoot->blocks[x0][y0] = index;
    }
    return &(gridBlocks[index].leaves[x1][y1]);
}

static void bin_lon (internal_coord_t *coord, double lon) {
    int32_t ilon = (lon * INT32_MAX) / 180;
    coord->xbin = ((uint32_t)ilon) >> 16; // unsigned type -> logical shift
    coord->xoff = ilon & 0xFFFF; 
}

static void bin_lat (internal_coord_t *coord, double lat) {
    int32_t ilat = (lat * INT32_MAX) / 90;
    coord->ybin = ((uint32_t)ilat) >> 16; // unsigned type -> logical shift
    coord->yoff = ilat & 0xFFFF; 
}

// Considering ocean covers 70 percent of the earth and the total number of theoretical 
// grid bins is 2^16, we would estimate (UINT16_MAX / 3) ~= 22000 blocks actually used.
// Experiment gives a similar but somewhat higher number of ~24000 grid blocks actually used 
// when loading the entire planet.pbf.
#define MAX_GRID_BLOCKS (30000)
// Max number of node/way blocks. This is probably an underestimate due to underfilled node blocks.
// The ratio of node to way block sizes should make this value also usable for ways.
#define NUM_BLOCKS (MAX_GRID_BLOCKS * 255 * 255)

void handle_way (OSMPBF__Way *way) {
}

static void handle_node (OSMPBF__Node *node) {
    internal_coord_t coord;
    double lat = node->lat * 0.0000001;
    double lon = node->lon * 0.0000001;
    bin_lat(&coord, lat);
    bin_lon(&coord, lon);
//    printf("lat %.5lf lon %.5lf x %d:%d y %d:%d\n", lat, lon, 
//        coord.xbin, coord.xoff, coord.ybin, coord.yoff);
    nodeStems[node->id].xbin = coord.xbin;
    nodeStems[node->id].ybin = coord.ybin;
    GridLeaf *leaf = get_grid_leaf(&coord);
}

/*
    if (leaf->nodes == 0) 
        leaf->nodes = new_node_block();
    NodeBlock *nblock = &(nodeBlocks[leaf->nodes]);
    // Fast-forward to end of linked list (actually we should insert new blocks at the head)
    while (nblock->next != NEXT_NONE)
        nblock = &(nodeBlocks[nblock->next]);
    // Insert a new block at the head if this block is full.
    if (nblock->nodes[NODE_BLOCK_SIZE - 1] != NODE_ID_UNUSED) {
        uint32_t nbidx = new_node_block();
        nblock = &(nodeBlocks[nbidx]);
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

void fillFactor () {
    int empty = 0;
    int used = 0;
    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            gridRoot->blocks[i][j] == 0 ? empty++ : used++ ;
        }
    }
    printf("index grid: %d empty %d used, %.2f%% full\n", 
        empty, used, ((double)used)/(used+empty) * 100); 
}

int main (int argc, const char * argv[]) {

    if (argc < 2) die("usage: cosm input.osm.pbf [database_dir]");
    const char *filename = argv[1];
    if (argc > 2) database_path = argv[2];
    gridRoot   = map_file("grid_root",   sizeof(GridRoot));
    gridBlocks = map_file("grid_blocks", sizeof(GridBlock) * MAX_GRID_BLOCKS);
    nodeStems  = map_file("node_stems",  sizeof(NodeStem)  * MAX_NODE_ID);
    nodeBlocks = map_file("node_blocks", sizeof(NodeBlock) * NUM_BLOCKS);
    wayBlocks  = map_file("way_blocks",  sizeof(WayBlock)  * NUM_BLOCKS);

    osm_callbacks_t callbacks;
    callbacks.way = &handle_way;
    callbacks.node = &handle_node;
    fillFactor();
    scan_pbf(filename, &callbacks);
    fillFactor();
    return EXIT_SUCCESS;

// TODO check fill proportion of grid blocks

}



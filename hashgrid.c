/* hashgrid.c */

#include "geometry.h"
#include <malloc.h>
#include <stdint.h>

typedef struct {
    double bin_size_meters;
    int grid_dim;
    coord_t bin_size;
    // http://stackoverflow.com/a/859694/778449
    // cdecl> declare items as pointer to array 10 of pointer to void
    // void *(*items)[10]
    uint16_t (*counts)[]; // 2D array of counts
    void *(*items)[];     // 2D array of void pointers
} HashGrid;

static inline int bin_for_coord (HashGrid *hg, coord_t *coord) {
    int xbin = coord->x / (hg->bin_size.x);
    int ybin = coord->y / (hg->bin_size.y);
    return ybin * hg->grid_dim + xbin;
}

void HashGrid_init (HashGrid *hg, double bin_size_meters, int grid_dim) {
    hg->grid_dim = grid_dim;
    hg->bin_size_meters = bin_size_meters;
    coord_from_meters (&(hg->bin_size), bin_size_meters, bin_size_meters);
    hg->counts = malloc(sizeof(uint16_t) * grid_dim * grid_dim);
    hg->items  = malloc(sizeof(void*) * grid_dim * grid_dim);
    uint16_t (*counts)[grid_dim] = hg->counts;
    void *(*items)[grid_dim] = hg->items;
    for (int y = 0; y < grid_dim; ++y) {
        for (int x = 0; x < grid_dim; ++x) {
            counts[y][x] = 0;
            items [y][x] = NULL;
        }
    }
}

void HashGrid_count (HashGrid *hg, coord_t *coord) {
    *((uint16_t*)(hg->counts) + bin_for_coord(hg, coord)) += 1;
}

static void free_bins (HashGrid *hg) {

}

void HashGrid_teardown(HashGrid *hg) {
    free_bins (hg);
    free (hg->counts);
    free (hg->items);
}


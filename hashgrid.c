/* hashgrid.c */
#include "hashgrid.h"

#include "geometry.h"
#include "tdata.h"
#include "config.h"
#include <syslog.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h> // be sure to link with math library (-lm) 

// Opaque struct, typedef in header
struct HashGrid {
    int     grid_dim;
    double  bin_size_meters;
    coord_t bin_size;
    int     n_items;
    int     (*counts)[]; // 2D array of counts
    int     *(*bins)[];  // 2D array of int pointers
    int     *items;      // array containing all the binned items, aliased by the bins array
    coord_t *coords;     // the array of coords that were indexed (note: may have been deallocated by caller)
};

// Opaque struct, typedef in header
struct HashGridResult {
    HashGrid *hg;
    coord_t coord;              // the query coordinate
    double radius_meters;       // query radius in meters
    coord_t min, max;           // defines a bounding box around the result in projected brads
    int xmin, xmax, ymin, ymax; // bins that correspond to the bounding box
    int x, y, i;                // current position within the hashgrid for iterating over results
    bool has_next;
}; 

// http://stackoverflow.com/a/859694/778449
// cdecl> declare items as pointer to array 10 of pointer to void
// void *(*items)[10]

// treat as unsigned to get 0-wrapping right? use only powers of 2 and bitshifting/masking to bin?
// ints are a circle coming back around to 0 after -1, 
// masking only the last B bits should wrap perfectly at both 0 and +180 degrees (except that the internal coords are projected).
// adding brad coords with overflow should also work, but you have to make sure overflow is happening (-fwrapv?)
static inline int xbin (HashGrid *hg, coord_t *coord) {
    int x = abs(coord->x / (hg->bin_size.x)); 
    x %= hg->grid_dim;
    T printf("binning x coord %d, bin is %d \n", coord->x, x);
    return x;
}

static inline int ybin (HashGrid *hg, coord_t *coord) {
    int y = abs(coord->y / (hg->bin_size.y));
    y %= hg->grid_dim;
    T printf("binning y coord %d, bin is %d \n", coord->y, y);
    return y;
}

void HashGrid_query (HashGrid *hg, HashGridResult *result, coord_t coord, double radius_meters) {
    result->coord = coord;
    result->radius_meters = radius_meters;
    coord_t radius;
    coord_from_meters (&radius, radius_meters, radius_meters);
    result->hg = hg;
    result->min.x = (coord.x - radius.x); // misbehaves at 0? need wrap macro?
    result->min.y = (coord.y - radius.y);
    result->max.x = (coord.x + radius.x); // coord_subtract / coord_add
    result->max.y = (coord.y + radius.y);
    result->xmin = xbin (hg, &(result->min));
    result->ymin = ybin (hg, &(result->min));
    result->xmax = xbin (hg, &(result->max));
    result->ymax = ybin (hg, &(result->max));
    result->x = result->xmin;
    result->y = result->ymin;
    result->i = 0; // should start at -1, which will increment to 0?
    result->has_next = true;
}

int HashGridResult_next (HashGridResult *r) {
    if ( ! (r->has_next))
        return -1;
    int (*counts)[r->hg->grid_dim] = r->hg->counts;
    int *(*bins)[r->hg->grid_dim] = r->hg->bins;
    r->i += 1;
    if (r->i >= counts[r->y][r->x]) {
        r->i = 0;
        if (r->x == r->xmax) { // note '==': inequalities do not work due to wrapping
            r->x = r->xmin;
            if (r->y == r->ymax) {
                r->has_next = false;
                return -1;
            } else {
                r->y = (r->y + 1) % r->hg->grid_dim;
            }
        } else {
            r->x = (r->x + 1) % r->hg->grid_dim;
        }
    }
    int item = bins[r->y][r->x][r->i];
    // printf ("x=%d y=%d i=%d item=%d ", r->x, r->y, r->i, item);
    return item;
}

/*
  Pre-filter the results, removing most false positives using a bounding box. This will only work if 
  the initial coordinate list is still available (was not freed or did not go out of scope). We 
  could also return a boolean to indicate whether there is a result, and have an out-parameter 
  for the index.
  The hashgrid can provide many false positives, but no false negatives (what is the term?).
  A bounding box or the squared distance can both be used to filter points. 
  Note that most false positives are quite far away so a bounding box is effective.
*/
int HashGridResult_next_filtered (HashGridResult *r, double *distance) {
    int item;
    while ((item = HashGridResult_next(r)) != -1) {
        coord_t *coord = r->hg->coords + item;
        latlon_t latlon;
        latlon_from_coord (&latlon, coord);
        // printf ("%f,%f\n", latlon.lat, latlon.lon);
        if (coord->x > r->min.x && coord->x < r->max.x && 
            coord->y > r->min.y && coord->y < r->max.y) {
            // item's coordinate is within bounding box, calculate actual distance
            *distance = coord_distance_meters (&(r->coord), coord);
            //printf ("%d,%d,%f\n", coord->x, coord->y, *distance);
            if (*distance < r->radius_meters) {
                return item;
            }
        }
    }
    return -1;
}

void HashGrid_init (HashGrid *hg, int grid_dim, double bin_size_meters, coord_t *coords, int n_items) {
    // Initialize all struct members.
    hg->grid_dim = grid_dim;
    hg->coords = coords;
    hg->bin_size_meters = bin_size_meters;
    coord_from_meters (&(hg->bin_size), bin_size_meters, bin_size_meters);
    hg->n_items = n_items;
    hg->counts = malloc(sizeof(int)  * grid_dim * grid_dim);
    hg->bins   = malloc(sizeof(int*) * grid_dim * grid_dim);
    hg->items  = malloc(sizeof(int) * n_items);
    // Initalize all dynamically allocated arrays.
    int (*counts)[grid_dim] = hg->counts;
    int  *(*bins)[grid_dim] = hg->bins;
    for (int y = 0; y < grid_dim; ++y) {
        for (int x = 0; x < grid_dim; ++x) {
            counts[y][x] = 0;
            bins  [y][x] = NULL;
        }
    }
    // Count the number of items that will fall into each bin.
    for (int c = 0; c < n_items; ++c) {
        T printf("binning coordinate x=%d y=%d \n", (coords + c)->x, (coords + c)->y);
        counts[ybin(hg, coords + c)][xbin(hg, coords + c)] += 1;
    }
    // Set bin pointers to alias subregions of the items array (which will be filled later).
    int *bin = hg->items; 
    for (int y = 0; y < grid_dim; ++y) {
        for (int x = 0; x < grid_dim; ++x) {
            bins[y][x] = bin;
            bin += counts[y][x];
            // Reset bin item count for reuse when filling up bins.
            counts[y][x] = 0; 
        }
    }
    // Add the item indexes to the bins, which are pointers to regions within the items array.
    for (int c = 0; c < n_items; ++c) {
        coord_t *coord = coords + c;
        int x = xbin (hg, coord);
        int y = ybin (hg, coord);
        int i = counts[y][x];
        bins[y][x][i] = c;
        counts[y][x] += 1;
    }
}

void HashGrid_dump (HashGrid* hg) {
    printf ("Hash Grid %dx%d \n", hg->grid_dim, hg->grid_dim);
    printf ("bin size: %f meters \n", hg->bin_size_meters);
    printf ("number of items: %d \n", hg->n_items);
    int (*counts)[hg->grid_dim] = hg->counts;
    int  *(*bins)[hg->grid_dim] = hg->bins;
    int *items = hg->items;
    // Grid of counts
    int total = 0;
    for (int y = 0; y < hg->grid_dim; ++y) {
        for (int x = 0; x < hg->grid_dim; ++x) {
            printf ("%2d ", counts[y][x]);
            total += counts[y][x];
        }
        printf ("\n");
    }
    printf ("total of all counts: %d", total);
    // Bins
    for (int y = 0; y < hg->grid_dim; ++y) {
        for (int x = 0; x < hg->grid_dim; ++x) {
            printf ("Bin [%02d][%02d] ", y, x);
            for (int i = 0; i < counts[y][x]; ++i) {
                printf ("%d ", bins[y][x][i]);
            }
            printf ("\n");
        }
    }
    printf ("\n");
}

void HashGrid_teardown(HashGrid *hg) {
    // Free up dynamically allocated arrays. 
    // Individual bins do not need to be freed separately from items.
    free (hg->counts);
    free (hg->bins);
    free (hg->items);
}

static void geometry_test (latlon_t *lls, int n) {
    for (int i = 0; i < n; ++i) {
        latlon_dump (lls + i);
        coord_t coord;
        coord_from_latlon(&coord, lls + i);
        coord.x = -coord.x;
        latlon_t ll;
        latlon_from_coord (&ll, &coord);
        latlon_dump (&ll);        
        coord_dump (&coord);
        coord_from_latlon(&coord, &ll);
        coord_dump (&coord);
    }
}

// Test HashGrid
int main(int argc, char** argv) {
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("hashgrid", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    tdata_t tdata;
    tdata_load (RRRR_INPUT_FILE, &tdata);
    // geometry_test (tdata.stop_coords, tdata.n_stops);
    HashGrid hg;
    coord_t coords[tdata.n_stops];
    for (int c = 0; c < tdata.n_stops; ++c) {
        coord_from_latlon(coords + c, tdata.stop_coords + c);
    }
    HashGrid_init (&hg, 100, 500.0, coords, tdata.n_stops);
    HashGrid_dump (&hg);
    coord_t qc;
    coord_from_lat_lon (&qc, 52.37790, 4.89787);
    coord_dump (&qc);
    double radius_meters = 150;
    HashGridResult result;
    HashGrid_query (&hg, &result, qc, radius_meters);
    int item;
    double distance;
    while ((item = HashGridResult_next_filtered(&result, &distance)) != -1) {
        latlon_t *ll = tdata.stop_coords + item;
        // latlon_dump (tdata.stop_coords + item);
        printf ("%d,%f,%f,%f\n", item, ll->lat, ll->lon, distance);
    }
    HashGrid_teardown (&hg);
    return 0;
}


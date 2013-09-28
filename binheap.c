/* adapted from the binheap class in OpenTripPlanner */
/* along with dynamic allocation in slab.c, a major enabler for fast MOA* searches. */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "util.h"

#define BINHEAP_GROW_FACTOR 2.0
#define BINHEAP_DEFAULT_CAPACITY 1000000
#define BINHEAP_MIN_CAPACITY 10

static float *prio;
static void  **elem;
static int   size; 
static int   capacity;

void binheap_new (int cap) {
    if (cap == 0) cap = BINHEAP_DEFAULT_CAPACITY;
    capacity = cap < BINHEAP_MIN_CAPACITY ? BINHEAP_MIN_CAPACITY : cap;
    elem = malloc (sizeof(void*) * (capacity + 1)); // 1-based indexing
    prio = malloc (sizeof(void*) * (capacity + 1)); // 1-based indexing
    size = 0;
    prio[0] = -INFINITY; // set sentinel
}
    
bool binheap_empty () { return size <= 0; }

float binheap_peek_min_key () {
	if (size > 0) return prio[1];
	else die ("An empty queue does not have a minimum key.");
	return NAN;
}
    
void *binheap_peek_min () {
	if (size > 0) return elem[1];
	else return NULL;
}
    
// public void rekey(T e, double p) { UNIMPLEMENTED }

void binheap_dump () {
	for (int i=0; i<=capacity; i++) {
    	printf("%d\t%f\t%s\t%s\n", i, prio[i], elem[i], (i > size) ? "(UNUSED)" : "");
	}
	printf("-----------------------\n");
}
    
/* empties the queue in one operation */
inline void binheap_reset () { size=0; } 

/* needs some work */
void *expand_array (void *ary, size_t elem_size) {
    int array_length = capacity + 1;
    void *new = malloc (array_length * elem_size);
    memcpy (new, ary, capacity); // sizes are not right
    free (ary);
    return new;
}

static void resize (int cap) {
	printf ("Growing queue to size %d\n", cap);
	if (size > cap) die ("BinHeap contains too many elements to fit in new capacity.");
	capacity = cap;
	prio = expand_array (prio, sizeof(float));
	elem = expand_array (elem, sizeof(void*));
}

void binheap_insert (void *e, float p) {
    int i;
    size += 1;
    if (size > capacity) resize((int)(capacity * BINHEAP_GROW_FACTOR));
    for (i = size; prio[i/2] > p; i /= 2) {
        elem[i] = elem[i/2];
        prio[i] = prio[i/2];
    }
    elem[i] = e;
    prio[i] = p;
}    
    
void *binheap_extract_min () {
    int   i, child;
    void *minElem  = elem[1];
    void *lastElem = elem[size];
    float lastPrio = prio[size];
    if (size <= 0) return NULL;
	size -= 1;
    for (i=1; i*2 <= size; i=child) {
        child = i*2;
        if (child != size && prio[child+1] < prio[child]) child++;
        if (lastPrio > prio[child]) {
            elem[i] = elem[child];
            prio[i] = prio[child];
        } else break;
    }
    elem[i] = lastElem;
    prio[i] = lastPrio;
    return minElem;
}
    


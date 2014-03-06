/* Copyright 2013 Bliksem Labs. See the LICENSE file at the top-level directory of this distribution and at https://github.com/bliksemlabs/rrrr/. */

/* slab.c */

/*
  An allocator that services many small incremental allocations by slicing up a few huge allocations.
  All the small allocations can then be freed in an O(1) blanket deallocation; this is basically a stack.
  This is a major enabler for fast MOA* routing where many states must be dynamically allocated.

  We could either reset to a marked intermediate point or always reset completely.
  We could either leave the slabs allocated at the high water mark or not (wait to call shrink).
  This could also be a pool of equally sized objects, i.e. each slab contains an array and a next pointer.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include "util.h"

#define DEFAULT_SLAB_SIZE (1024 * 1024 * 1)

static size_t slab_size;
static struct slab *head;
static struct slab *last;
static size_t total_size;
static void *cur;  // the current byte within the chain of slabs
static void *end;  // the last byte within the chain of slabs

/* Slabs are chained into a linked list. All slabs store their beginning address to allow subsequent deallocation. */
struct slab {
    void *begin; // the beginning of this slab of memory
    struct slab *next; // the next slab in the chain
};

/* Allocate a new slab, adding it to the end of the chain and updating current and end position pointers accordingly. */
struct slab *slab_new () {
    struct slab *slab = malloc (sizeof(struct slab));
    if (slab == NULL) die ("cannot allocate slab.");
    if (last != NULL) last->next = slab;
    last = slab;
    last->begin = malloc (slab_size);
    if (last->begin == NULL) die ("cannot allocate slab.");
    last->next = NULL;
    cur = last->begin;
    end = cur + slab_size;
    total_size += slab_size;
    printf ("allocated new slab at %p. total allocated is now %zd.\n", last->begin, total_size);
    return last;
}

void slab_next () {
    last = last->next;
    cur = last->begin;
    end = cur + slab_size;
}

/* Initialize the slab allocator system as a whole. */
void slab_init (size_t size) {
    slab_size = (size ? size : DEFAULT_SLAB_SIZE);
    total_size = 0;
    last = NULL;
    head = slab_new ();
}

/* Deallocate the given slab and all following it in the linked list. */
void slab_destroy_chain (struct slab *slab) {
    while (slab != NULL) {
        struct slab *next = slab->next; // avoid accessing deallocated memory, just in case
        printf ("deallocating slab at %p.\n", slab->begin);
        free (slab->begin);
        free (slab);
        slab = next;
    }
}

/* Deallocate all slabs. */
void slab_destroy () { slab_destroy_chain (head); }

void slab_free () {
/*
    size_t size = slab_size;
    slab_destroy ();
    slab_init (size);
*/
    printf ("slab free\n");
    cur = head->begin;
    end = cur + slab_size;
    last = head;
}

/* Allocating more than the slab size will cause bad things to happen. Do not do that. */
void *slab_alloc (size_t bytes) {
    if (bytes > slab_size) return NULL;
    // align pointer here based on amount to be allocated
    if (cur + bytes >= end) {
        if (last->next == NULL) {
            slab_new ();
        } else {
            slab_next ();
        }
    }
    void *ret = cur;
    cur += bytes;
    return ret;
}

typedef struct {
    int a;
    float b;
    bool c;
} test_s;



/* TESTING */

#define PASSES 10
#define ALLOCS (1000 * 1000 * 12)
#define SLAB_SIZE (1024 * 1024 * 2)
// do not allocate this huge array on the stack, it will blow up (thanks valgrind)
static test_s *tsps[ALLOCS];

int test_slab (int argc, char **argv) {
    // track runtime
    struct timeval t0, t1;

    // malloc version
    gettimeofday (&t0, NULL);
    for (int p = 0; p < PASSES; ++p) {
        for (int i = 0; i < ALLOCS; ++i) {
            test_s *ts = malloc (sizeof(test_s));
            ts->a = i;
            ts->b = i;
            ts->c = (i % 2 == 0);
            tsps[i] = ts;
        }
        for (int i = 0; i < ALLOCS; ++i) {
            free (tsps[i]);
        }
    }
    gettimeofday (&t1, NULL);
    double mdt = ((t1.tv_usec + 1000000 * t1.tv_sec) - (t0.tv_usec + 1000000 * t0.tv_sec)) / 1000000.0;

    // slab alloc version
    gettimeofday (&t0, NULL);
    slab_init (SLAB_SIZE);
    for (int p = 0; p < PASSES; ++p) {
        slab_free ();
        for (int i = 0; i < ALLOCS; ++i) {
            test_s *ts = slab_alloc (sizeof(test_s));
            ts->a = i;
            ts->b = i;
            ts->c = (i % 2 == 0);
            tsps[i] = ts;
        }
    }
    gettimeofday (&t1, NULL);
    double sdt = ((t1.tv_usec + 1000000 * t1.tv_sec) - (t0.tv_usec + 1000000 * t0.tv_sec)) / 1000000.0;

    /* Check that results are coherent, and collapse the wavefunction. */
    for (int i = 0; i < ALLOCS; ++i) {
        test_s ts = *(tsps[i]);
        if (ts.a != i || ts.b != i || ((i % 2 == 0) != ts.c)) {
            printf ("ERR ptr=%p, i=%d, a=%d, b=%f, c=%s\n", tsps[i], i, ts.a, ts.b, ts.c ? "EVEN" : "ODD");
        }
    }

    fprintf (stderr, "%f sec malloc, %f sec slab, speedup %f\n", mdt, sdt, mdt/sdt);
    return 0;
}


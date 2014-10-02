/* dedup.h : deduplicates strings using a hash table that maps them to integer IDs. */

#include <stdint.h>
#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"

void Dedup_clear();
void Dedup_init();
void Dedup_print();
uint32_t Dedup_dedup (char *key);
OSMPBF__StringTable *Dedup_string_table ();


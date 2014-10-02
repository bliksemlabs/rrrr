/* tags.c */
#include "tags.h"
#include <string.h>
#include <stdio.h>
#include "pbf.h"

// http://taginfo.openstreetmap.org/keys

typedef struct {
    char *key;
    int  len;     // works as long as len >= size of first dimension of vals
    char **vals;  // strings could also be fixed-width
} KVTable;


// This could just be one giant char*[] with null separators

static KVTable tables[] = {
    {
        "highway", 32, (char *[]) {
            "residential",
            "service",
            "track",
            "unclassified",
            "footway",
            "tertiary",
            "path",
            "secondary",
            "primary",
            "bus_stop",
            "crossing",
            "turning_circle",
            "cycleway",
            "trunk",
            "traffic_signals",
            "living_street",
            "motorway",
            "steps",
            "motorway_link",
            "road",
            "pedestrian",
            "trunk_link",
            "primary_link",
            "stop",
            "secondary_link",
            "motorway_junction",
            "tertiary_link",
            "construction",
            "give_way",
            "bridleway",
            "platform",
            "mini_roundabout"
        }
    },
    {
        "building", 8, (char *[]) {
            "yes",
            "house",
            "residential",
            "garage",
            "hut",
            "industrial",
            "commercial",
            "retail"
        }
    },
    {
        "landuse", 8, (char *[]) {
            "forest",
            "residential",
            "grass",
            "farmland",
            "meadow",
            "farm",
            "reservoir",
            "industrial"
        }
    }, 
    {
        "surface", 12, (char *[]) {
            "asphalt",
            "unpaved",
            "paved",
            "gravel",
            "ground",
            "dirt",
            "grass",
            "concrete",
            "paving_stones",
            "sand",
            "cobblestone",
            "compacted"
        }
    },
    {
        "amenity", 8, (char *[]) {
            "parking",
            "place_of_worship",
            "school",
            "restaurant",
            "bench",
            "fuel",
            "post_box",
            "bank"
        }
    },
    {
        "power", 8, (char *[]) {
            "tower",
            "pole",
            "line",
            "generator",
            "minor_line",
            "sub_station",
            "substation",
            "station"
        }
    }, 
    {
        "traffic_calming", 5, (char *[]) {
            "bump",
            "hump",
            "table",
            "yes",
            "island"
        }
    }, 
    {
        "railway", 8, (char *[]) {
            "rail",
            "level_crossing",
            "abandoned",
            "station",
            "buffer_stop",
            "tram",
            "switch",
            "platform"
        }
    }, 
    {
        "service", 8, (char *[]) {
            "parking_aisle",
            "driveway",
            "alley",
            "spur",
            "yard",
            "siding",
            "drive-through",
            "emergency_access"
        }
    }, 
    {
        "access", 8, (char *[]) {
            "private",
            "yes",
            "no",
            "permissive",
            "destination",
            "agricultural",
            "customers",
            "designated"
        }
    }, 
    {
        "crossing", 6, (char *[]) {
            "uncontrolled",
            "traffic_signals",
            "unmarked",
            "island",
            "zebra",
            "no"
        }
    }, 
    {
        "footway", 8, (char *[]) {
            "sidewalk",
            "crossing",
            "both",
            "none",
            "right",
            "left",
            "no",
            "yes"
        }
    }, 
    {NULL, 0, NULL} // sentinel
};


/* retain by string:
name
addr:*
all nodes and edges point into a char array: kvkvkvkv\0key=value\0key=value
it is tempting to try to save all needed tags as numbers, but remember we need free-text for names etc.
*/

char *free_text_keys[] = {
    "addr:postcode",
    "addr:postcode:left",
    "addr:postcode:right",
    "addr:housenumber",
    "addr:street",
    "addr:city",
    "addr:country",
    "addr:full",
    "addr:state",
    "amenity",
    "bicycle",
    "bridge",
    "building",
    "cycleway",
    "embankment",
    "exit_to",
    "footway",
    "highway",
    "landuse",
    "lanes",
    "maxspeed",
    "name",
    "oneway",
    "phone",
    "public_transport",
    "railway",
    "service",
    "surface",
    "tunnel",
    "website",
    "zip_left",
    "zip_right",
    NULL // list terminator
};

int8_t encode_tag (ProtobufCBinaryData key, ProtobufCBinaryData val) {
    int code = 1;
    for (KVTable *table = &tables[0]; table->key != NULL; table++) {
        if (memcmp(table->key, key.data, key.len) == 0) { // Found key
            for (int v = 0; v < table->len; v++, code++) {
                if (memcmp(table->vals[v], val.data, val.len) == 0) { // Found key+val
                    return code;
                }
            }
            break; // try free-text
        }
        code += table->len;
    }
    // Key-value combination was not found, next try free-text keys
    code = -1;
    for (char **k = &(free_text_keys[0]); *k != NULL; k++, code--) {
        if (memcmp(*k, key.data, key.len) == 0) { // Found key
            return code;
        }
    }
    return 0; // No code found for this KV pair
}

/* Return the number of characters consumed. We could also just return the new position of the pointer? */
size_t decode_tag (char *buf, KeyVal *kv) {
    char *c = buf;
    int8_t code = (int8_t) *(c++);
    if (code == 0) {
        kv->key = c;
        while (*c != '\0') c++;
        kv->val = (++c);
        while (*c != '\0') c++;
        c++;
    } else if (code < 0) {
        code += 1; // table codes are one-based, shift toward zero
        kv->key = free_text_keys[-code];
        kv->val = c;
        while (*c != '\0') c++;
        c++;
    } else {
        code -= 1; // table codes are one-based, shift toward zero
        KVTable *table = &tables[0];
        while (code >= table->len) { 
            code -= table->len;
            table++;
            if (table->key == NULL) return -1; // table overrun, invalid input code
        }
        // code is in the current table
        kv->key = table->key;
        kv->val = table->vals[code];
    }
    size_t n_decoded = c - buf;
    // printf ("\ndecoded %zd bytes: ", n_decoded);
    // fwrite (buf, n_decoded, 1, stdout);
    // fputc ('\n', stdout);
    return n_decoded;
}



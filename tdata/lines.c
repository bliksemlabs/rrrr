#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "string_pool.h"
#include "lines.h"

#define REALLOC_EXTRA_SIZE     16

ret_t
tdata_lines_init (tdata_lines_t *lines, tdata_string_pool_t *pool)
{
    lines->line_ids = NULL;
    lines->line_codes = NULL;
    lines->line_names = NULL;
    lines->line_colors = NULL;
    lines->line_colors_text = NULL;
    lines->operator_for_line = NULL;
    lines->physical_mode_for_line = NULL;

    lines->pool = pool;

    lines->size = 0;
    lines->len = 0;

    return ret_ok;
}

ret_t
tdata_lines_mrproper (tdata_lines_t *lines)
{
    if (unlikely (lines->size == 0 && lines->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (lines->line_ids)
        free (lines->line_ids);
    
    if (lines->line_codes)
        free (lines->line_codes);
    
    if (lines->line_names)
        free (lines->line_names);
 
    if (lines->line_colors)
        free (lines->line_colors);
     
    if (lines->line_colors_text)
        free (lines->line_colors_text);
    
    if (lines->operator_for_line)
        free (lines->operator_for_line);
    
    if (lines->physical_mode_for_line)
        free (lines->physical_mode_for_line);
 
    return tdata_lines_init (lines, lines->pool);
}

ret_t
tdata_lines_ensure_size (tdata_lines_t *lines, lineidx_t size)
{
    void *p;

    /* Maybe it doesn't need it
     * if buf->size == 0 and size == 0 then buf can be NULL.
     */
    if (size <= lines->size)
        return ret_ok;

    /* If it is a new buffer, take memory and return
     */
    if (lines->line_ids == NULL) {
        lines->line_ids               = (uint32_t *) calloc (size, sizeof(uint32_t));
        lines->line_codes             = (uint32_t *) calloc (size, sizeof(uint32_t));
        lines->line_names             = (uint32_t *) calloc (size, sizeof(uint32_t));
        lines->line_colors            = (uint32_t *) calloc (size, sizeof(uint32_t));
        lines->line_colors_text       = (uint32_t *) calloc (size, sizeof(uint32_t));
        lines->operator_for_line      = (opidx_t *)  calloc (size, sizeof(opidx_t));
        lines->physical_mode_for_line = (pmidx_t *)  calloc (size, sizeof(pmidx_t));
        
        if (unlikely (lines->line_ids == NULL ||
                      lines->line_codes == NULL ||
                      lines->line_names == NULL ||
                      lines->line_colors == NULL ||
                      lines->line_colors_text == NULL ||
                      lines->operator_for_line == NULL ||
                      lines->physical_mode_for_line == NULL)) {
            return ret_nomem;
        }
        lines->size = size;
        return ret_ok;
    }

    /* It already has memory, but it needs more..
     */
    p = realloc (lines->line_ids, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->line_ids = (uint32_t *) p;

    p = realloc (lines->line_codes, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->line_codes = (uint32_t *) p;

    p = realloc (lines->line_names, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->line_names = (uint32_t *) p;

    p = realloc (lines->line_colors, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->line_colors = (uint32_t *) p;

    p = realloc (lines->line_colors_text, sizeof(uint32_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->line_colors_text = (uint32_t *) p;

    p = realloc (lines->operator_for_line, sizeof(opidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->operator_for_line = (opidx_t *) p;

    p = realloc (lines->physical_mode_for_line, sizeof(pmidx_t) * size);
    if (unlikely (p == NULL)) {
        return ret_nomem;
    }
    lines->physical_mode_for_line = (pmidx_t *) p;

    lines->size = size;
    
    return ret_ok; 
}

ret_t
tdata_lines_add (tdata_lines_t *lines,
                     const char **id,
                     const char **code,
                     const char **name,
                     const char **color,
                     const char **color_text,
                     const opidx_t *operator_for_line,
                     const pmidx_t *physical_mode_for_line,
                     const lineidx_t size,
                     lineidx_t *offset)
{
    int available;

    if (unlikely (lines->size == 0 && lines->len > 0)) {
        /* The memory has arrived via a memory mapping */
        return ret_deny;
    }

    if (unlikely (size == 0)) {
        return ret_ok;
    }

    /* Get memory
     */
    available = lines->size - lines->len;

    if ((lineidx_t) available < size) {
        if (unlikely (tdata_lines_ensure_size (lines, (size - available) + REALLOC_EXTRA_SIZE) != ret_ok)) {
            return ret_nomem;
        }
    }

    /* Copy
     */
    memcpy (lines->operator_for_line + lines->len, operator_for_line, sizeof(opidx_t) * size);
    memcpy (lines->physical_mode_for_line + lines->len, physical_mode_for_line, sizeof(pmidx_t) * size);

    available = size;

    while (available) {
        lineidx_t idx;

        available--;
        idx = lines->len + available;
        tdata_string_pool_add (lines->pool, id[available], strlen (id[available]) + 1, &lines->line_ids[idx]);
        tdata_string_pool_add (lines->pool, code[available], strlen (code[available]) + 1, &lines->line_codes[idx]);
        tdata_string_pool_add (lines->pool, name[available], strlen (name[available]) + 1, &lines->line_names[idx]);
        tdata_string_pool_add (lines->pool, color[available], strlen (color[available]) + 1, &lines->line_colors[idx]);
        tdata_string_pool_add (lines->pool, color_text[available], strlen (color_text[available]) + 1, &lines->line_colors_text[idx]);
    }

    if (offset) {
        *offset = lines->len;
    }    

    lines->len += size;

    return ret_ok;
}

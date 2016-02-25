#include "common.h"
#include "tdata_common.h"
#include "operators.h"
#include "physical_modes.h"
#include "string_pool.h"

typedef struct {
    uint32_t *line_ids;
    uint32_t *line_codes;
    uint32_t *line_names;
    uint32_t *line_colors;
    uint32_t *line_colors_text;
    opidx_t  *operator_for_line;
    pmidx_t  *physical_mode_for_line;

    tdata_operators_t *operators;
    tdata_physical_modes_t *physical_modes;
    tdata_string_pool_t *pool;

    opidx_t size; /* Total amount of memory */
    opidx_t len;  /* Length of the list   */
} tdata_lines_t;

ret_t tdata_lines_init (tdata_lines_t *lines, tdata_operators_t *operators, tdata_physical_modes_t *physical_modes, tdata_string_pool_t *pool);
ret_t tdata_lines_fake (tdata_lines_t *lines,
                     const uint32_t *line_ids,
                     const uint32_t *line_codes,
                     const uint32_t *line_names,
                     const uint32_t *line_colors,
                     const uint32_t *line_colors_text,
                     const opidx_t *operator_for_line,
                     const pmidx_t *physical_mode_for_line,
                     const uint32_t len);
ret_t tdata_lines_mrproper (tdata_lines_t *lines);
ret_t tdata_lines_ensure_size (tdata_lines_t *lines, lineidx_t size);
ret_t tdata_lines_add (tdata_lines_t *lines,
                           const char **id,
                           const char **code,
                           const char **name,
                           const char **color,
                           const char **color_text,
                           const opidx_t *operator_for_line,
                           const pmidx_t *physical_mode_for_line,
                           const lineidx_t size,
                           lineidx_t *offset);

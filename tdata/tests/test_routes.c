#include <stdio.h>
#include <stdint.h>
#include "routes.h"

int main(int argv, char *args[]) {
    tdata_string_pool_t pool;
    tdata_operators_t ops;
    tdata_physical_modes_t pms;
    tdata_lines_t lines;
    tdata_routes_t routes;

    opidx_t op_offset;
    pmidx_t pm_offset;
    lineidx_t line_offset;
    routeidx_t route_offset;

    (void)(argv);
    (void)(args);

    tdata_string_pool_init (&pool);
    tdata_operators_init (&ops, &pool);
    tdata_physical_modes_init (&pms, &pool);
    tdata_lines_init (&lines, &ops, &pms, &pool);
    tdata_routes_init (&routes, &lines);

    {
        const char *id = "cxx";
        const char *url = "http://connexxion.nl/";
        const char *name = "Connexxion";
        tdata_operators_add (&ops, &id, &url, &name, 1, &op_offset);
    }

    {
        const char *id[] = { "TRAM" };
        const char *name[] = { "Tram" };
        tdata_physical_modes_add (&pms, id, name, 1, &pm_offset);
    }

    {
        const char *id = "45";
        const char *code = "45";
        const char *name = "Leiden - Den Haag";
        const char *color = "FFFFFF";
        const char *color_text = "000000";
        tdata_lines_add (&lines, &id, &code, &name, &color, &color_text, &op_offset, &pm_offset, 1, &line_offset);
    }

    {
        tdata_routes_add (&routes, &line_offset, 1, &route_offset);
    }

    printf("%s %s %s\n", &ops.pool->pool[ops.operator_names[lines.operator_for_line[0]]],
                         &pms.pool->pool[pms.physical_mode_names[lines.physical_mode_for_line[0]]],
                         &lines.pool->pool[lines.line_names[0]]);

    tdata_lines_mrproper (&lines);
    tdata_physical_modes_mrproper (&pms);
    tdata_operators_mrproper (&ops);
    tdata_string_pool_mrproper (&pool);

    return 0;
}

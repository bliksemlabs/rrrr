#include <stdio.h>
#include <stdint.h>
#include "stop_times.h"

int main(int argv, char *args[]) {
    tdata_stop_times_t sps;

    uint32_t offset;

    (void)(argv);
    (void)(args);

    tdata_stop_times_init (&sps);

    {
        const rtime_t arrival[] = {0, 15, 30};
        const rtime_t departure[] = {0, 20, 30};
        tdata_stop_times_add (&sps, arrival, departure, 3, &offset);
    }

    tdata_stop_times_mrproper (&sps);

    return 0;
}

#include <stdio.h>
#include <stdint.h>
#include "transfers.h"

int main(int argv, char *args[]) {
    tdata_transfers_t transfers;

    (void)(argv);
    (void)(args);

    tdata_transfers_init (&transfers);

    {
        const spidx_t transfer_target_stops[] = {1, 0};
        const rtime_t transfer_durations[] = {30, 30};

        tdata_transfers_add (&transfers, transfer_target_stops, transfer_durations, 2, NULL);
    }

    tdata_transfers_mrproper (&transfers);

    return 0;
}

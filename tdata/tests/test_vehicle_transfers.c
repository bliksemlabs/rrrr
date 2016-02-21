#include <stdio.h>
#include <stdint.h>
#include "vehicle_transfers.h"

int main(int argv, char *args[]) {
    tdata_vehicle_transfers_t vts;

    uint32_t offset;

    (void)(argv);
    (void)(args);

    tdata_vehicle_transfers_init (&vts);

    {
        const vehicle_journey_ref_t vehicle_journey_transfers[] = {{0, 0}};
        tdata_vehicle_transfers_add (&vts, vehicle_journey_transfers, 1, &offset);
    }

    tdata_vehicle_transfers_mrproper (&vts);

    return 0;
}

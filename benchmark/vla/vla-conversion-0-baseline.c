#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    uint32_t grid_dim = 1000;
    uint32_t n_tests;
    uint32_t (*counts)[] = (uint32_t (*)[]) calloc(grid_dim * grid_dim, sizeof(uint32_t));
    uint32_t (*counts_test)[grid_dim] = counts;

    for (n_tests = 0; n_tests < 100000; ++n_tests) {
        for (uint32_t y = 0; y < grid_dim; ++y) {
            for (uint32_t x = 0; x < grid_dim; ++x) {
                counts_test[y][x] += 1;
            }
        }
    }

    return 0;
}

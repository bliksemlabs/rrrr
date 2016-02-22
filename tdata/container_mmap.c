#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "common.h"
#include "container_mmap.h"
#include "tdata_io_v4.h"

#define load_mmap(b, storage, type) \
    tdata_string_pool_fake (&container->container.storage, (type *) (((char *) b) + header->loc_##storage), header->n_##storage)

ret_t
tdata_container_mmap_init (tdata_container_mmap_t *container, const char *filename)
{
    struct stat st;
    tdata_header_t *header;
    int fd;

    tdata_container_init (&container->container);

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "The input file %s could not be found.\n", filename);
        return false;
    }

    if (stat(filename, &st) == -1) {
        fprintf(stderr, "The input file %s could not be stat.\n", filename);
        goto fail_close_fd;
    }

    if (st.st_size < (long) sizeof(header)) {
        fprintf(stderr, "The input file %s is too small.\n", filename);
        goto fail_close_fd;
    }

    /* we can cast off_t to size_t since we have checked it is > 0 */
    container->base = mmap(NULL, (size_t) st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    container->size = (size_t) st.st_size;
    if (container->base == MAP_FAILED) {
        fprintf(stderr, "The input file %s could not be mapped.\n", filename);
        goto fail_close_fd;
    }

    header = (tdata_header_t *) container->base;
    if( strncmp("TTABLEV4", header->version_string, 8) ) {
        fprintf(stderr, "The input file %s does not appear to be a timetable or is of the wrong version.\n", filename);
        goto fail_munmap_base;
    }

    container->container.timezone = header->timezone;
    container->container.calendar_start_time = header->calendar_start_time;
    container->container.utc_offset = header->utc_offset;
    container->container.n_days = header->n_days;

    /* We must close the file descriptor otherwise we will
     * leak it. Because mmap has created a reference to it
     * there will not be a problem.
     */
    close (fd);

    load_mmap (container->base, string_pool, char);

    return ret_ok;

fail_munmap_base:
    munmap(container->base, container->size);

fail_close_fd:
    close(fd);

    return ret_error;
}

ret_t
tdata_container_mmap_mrproper (tdata_container_mmap_t *container)
{
    tdata_container_mrproper (&container->container);

    munmap(container->base, container->size);

    container->base = NULL;

    container->size = 0;

    return ret_ok;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

#define DEVICE "/dev/asgn1"

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s reset\n", prog);
    printf("  %s stats\n", prog);
    printf("  %s mode <0|1>\n", prog);
    printf("  %s version\n", prog);
}

int main(int argc, char *argv[])
{
    int fd;
    int ret;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    if (strcmp(argv[1], "reset") == 0) {

        ret = ioctl(fd, ASGN1_RESET_BUFFER);

        if (ret < 0) {
            perror("ASGN1_RESET_BUFFER");
            close(fd);
            return 1;
        }

        printf("RESET_BUFFER: PASS\n");
    }

    else if (strcmp(argv[1], "stats") == 0) {

        struct asgn1_stats stats;

        memset(&stats, 0, sizeof(stats));

        ret = ioctl(fd, ASGN1_GET_STATS, &stats);

        if (ret < 0) {
            perror("ASGN1_GET_STATS");
            close(fd);
            return 1;
        }

        printf("GET_STATS:\n");
        printf("  open_count      = %d\n", stats.open_count);
        printf("  write_count     = %d\n", stats.write_count);
        printf("  read_count      = %d\n", stats.read_count);
        printf("  buffer_len      = %d\n", stats.buffer_len);
        printf("  last_write_size = %d\n", stats.last_write_size);
    }

    else if (strcmp(argv[1], "mode") == 0) {

        int mode;

        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }

        mode = atoi(argv[2]);

        ret = ioctl(fd, ASGN1_SET_MODE, &mode);

        if (ret < 0) {
            perror("ASGN1_SET_MODE");
            close(fd);
            return 1;
        }

        printf("SET_MODE: %d PASS\n", mode);
    }

    else if (strcmp(argv[1], "version") == 0) {

        char version[ASGN1_VERSION_LEN];

        memset(version, 0, sizeof(version));

        ret = ioctl(fd, ASGN1_GET_VERSION, version);

        if (ret < 0) {
            perror("ASGN1_GET_VERSION");
            close(fd);
            return 1;
        }

        printf("GET_VERSION: %s\n", version);
    }

    else {
        usage(argv[0]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

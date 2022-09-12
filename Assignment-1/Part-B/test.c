#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

int main() {
    int fd = open("/proc/partb_1_3", O_RDWR);
    sleep(10);

    close(fd);
    return 0;
}
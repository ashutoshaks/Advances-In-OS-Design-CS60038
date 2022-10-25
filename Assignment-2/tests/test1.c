#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_INT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_PRIO _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOW(0x10, 0x34, int32_t *)
#define PB2_GET_MIN _IOW(0x10, 0x35, int32_t *)
#define PB2_GET_MAX _IOW(0x10, 0x36, int32_t *)

void execute(int val[], int n, int prio[]) {
    int fd = open("/proc/partb_1_3", O_RDWR);
    int ret = ioctl(fd, PB2_SET_CAPACITY, &n);

    for (int i = 0; i < n; i++) {
        ret = write(fd, &val[i], sizeof(int));
        printf("[Proc %d] Write: %d, Return: %d\n", getpid(), val[i], ret);
        usleep(100);

        ret = write(fd, &prio[i], sizeof(int));
        printf("[Proc %d] Write: %d, Return: %d\n", getpid(), prio[i], ret);
        usleep(100);
    }
    for (int i = 0; i < n; i++) {
        int out;
        ret = read(fd, &out, sizeof(int));
        printf("[Proc %d] Read: %d, Return: %d\n", getpid(), out, ret);
        usleep(100);
    }
    close(fd);
}

int main() {
    int val_p[] = {0, 1, -2, 3, 4};
    int prio_p[] = {5, 2, 9, 2, 3};

    execute(val_p, 5, prio_p);

    return 0;
}
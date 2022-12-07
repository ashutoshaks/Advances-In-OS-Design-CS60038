/*
    Ashutosh Kumar Singh - 19CS30008
    Vanshita Garg - 19CS10064
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wait.h>

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_INT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_PRIO _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOR(0x10, 0x34, int32_t *)
#define PB2_GET_MIN _IOR(0x10, 0x35, int32_t *)
#define PB2_GET_MAX _IOR(0x10, 0x36, int32_t *)

struct obj_info {
    int32_t prio_que_size;  // current number of elements in priority queue
    int32_t capacity;       // maximum capacity of priority queue
};

void execute(int val[], int n, int prio[]) {
    int fd = open("/proc/cs60038_a2_grp3", O_RDWR);
    printf("fd: %d\n", fd);
    int ret;
    int sz = 105;
    if ((ret = ioctl(fd, PB2_SET_CAPACITY, &n)) < 0) {
        printf("Error, return value: %d, Errno: %d\n", ret, errno);
        return;
    }
    // int fd1 = open("/proc/cs60038_a2_grp3", O_RDWR);
    // if (fd1 < 0) {
    //     printf("Opening again, error, return value: %d, Errno: %d\n", fd1, errno);
    // }
    for (int i = 0; i < n; i++) {
        int ret = ioctl(fd, PB2_INSERT_INT, &val[i]);
        printf("[Proc %d] Write: %d, Return: %d, Errno: %d\n", getpid(), val[i], ret, errno);
        usleep(100);

        ret = ioctl(fd, PB2_INSERT_PRIO, &prio[i]);
        printf("[Proc %d] Write: %d, Return: %d, Errno: %d\n", getpid(), prio[i], ret, errno);
        usleep(100);
    }

    struct obj_info info;
    ret = ioctl(fd, PB2_GET_INFO, &info);
    printf("[Proc %d] Current Size: %d, Capacity: %d, Return: %d, Errno: %d\n", getpid(), info.prio_que_size, info.capacity, ret, errno);

    for (int i = 0; i < n; i++) {
        int out;
        ret = ioctl(fd, PB2_GET_MIN, &out);
        printf("[Proc %d] Read Min: %d, Return: %d, Errno: %d\n", getpid(), out, ret, errno);
        usleep(100);

        ret = ioctl(fd, PB2_GET_MAX, &out);
        printf("[Proc %d] Read Max: %d, Return: %d, Errno: %d\n", getpid(), out, ret, errno);
    }
    close(fd);
}

int main() {
    int val_p[] = {6, -7, 8, -9};
    int prio_p[] = {56, 38, 98, 27};

    execute(val_p, sizeof(val_p) / sizeof(int), prio_p);

    return 0;
}

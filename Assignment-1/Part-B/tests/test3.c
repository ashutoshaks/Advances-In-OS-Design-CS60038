#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>

void execute(int val[], int n, int prio[]) {
    int fd = open("/proc/partb_1_3", O_RDWR);
    printf("fd: %d\n", fd);
    char c = (char)(n);
    int ret;
    if ((ret = write(fd, &c, 1)) < 0) {
        printf("Error, return value: %d\n", ret);
        return;
    }
    int fd1 = open("/proc/partb_1_3", O_RDWR);
    if (fd1 < 0) {
        printf("Error, return value: %d\n", fd1);
    }
    for (int i = 0; i < n; i++) {
        int ret = write(fd, &val[i], sizeof(int));
        printf("[Proc %d] Write: %d, Return: %d\n", getpid(), val[i], ret);
        usleep(100);

        ret = write(fd, &prio[i], sizeof(int));
        printf("[Proc %d] Write: %d, Return: %d\n", getpid(), prio[i], ret);
        usleep(100);
    }
    for (int i = 0; i < n; i++) {
        int out;
        int ret = read(fd, &out, sizeof(int));
        printf("[Proc %d] Read: %d, Return: %d\n", getpid(), out, ret);
        usleep(100);
    }
    close(fd);
}

int main() {
    int val_p[] = {0, 1, -2};
    int prio_p[] = {5, 2, 9};

    execute(val_p, sizeof(val_p) / sizeof(int), prio_p);

    return 0;
}
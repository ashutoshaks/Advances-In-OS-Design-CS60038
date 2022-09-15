#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>

void execute(int val[], int n, int prio[]) {
    int fd = open("/proc/partb_1_3", O_RDWR);
    char c = (char)n;
    write(fd, &c, 1);
    for (int i = 0; i < n; i++) {
        int ret = write(fd, &val[i], sizeof(int));
        printf("[Proc %d] Write: %d, Return: %d\n", getpid(), val[i], ret);
        usleep(100);

        ret = write(fd, &prio[i], sizeof(int));
        printf("[Proc %d] Write: %d, Return: %d\n", getpid(), prio[i], ret);
        usleep(100);
    }
    for (int i = 0; i < n + 1; i++) {
        int out;
        int ret = read(fd, &out, sizeof(int));
        printf("[Proc %d] Read: %d, Return: %d\n", getpid(), out, ret);
        usleep(100);
    }
    close(fd);
}

int main() {
    // int val_p[] = {0, 1, -2, 3, 4};
    // int prio_p[] = {5, 2, 9, 2, 3};

    // int val_c[] = {6, -7, 8, -9, 10};
    // int prio_c[] = {56, 38, 98, 27, 98};

    int val_p[] = {0, 1, -2, 3};
    int prio_p[] = {5, 2, 9, 2};

    int val_c[] = {6, -7, 8, -9};
    int prio_c[] = {56, 38, 98, 27};

    int pid = fork();
    if (pid == 0) {
        execute(val_c, 4, prio_c);
    } else {
        execute(val_p, 4, prio_p);
        wait(NULL);
    }
    
    return 0;
}
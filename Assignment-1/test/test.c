#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>

int main() {
    int fd = open("/proc/partb_1_3", O_RDWR);
    char capacity = (char)10;
    write(fd, &capacity, sizeof(char));
    // write value then priority one by one
    int value = 1;
    int priority = 5;
    write(fd, &value, sizeof(int));
    write(fd, &priority, sizeof(int));
    value = -2;
    priority = 2;
    write(fd, &value, sizeof(int));
    write(fd, &priority, sizeof(int));
    value = 3;
    priority = 9;
    write(fd, &value, sizeof(int));
    write(fd, &priority, sizeof(int));
    value = 4;
    priority = 1;
    write(fd, &value, sizeof(int));
    write(fd, &priority, sizeof(int));
    value = -5;
    priority = 2;
    write(fd, &value, sizeof(int));
    write(fd, &priority, sizeof(int));

    // make read calls
    for (int i = 0; i < 5; i++) {
        int read_value;
        read(fd, &read_value, sizeof(int));
        printf("read value: %d", read_value);
    }

    close(fd);
    return 0;
}
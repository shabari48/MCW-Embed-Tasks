#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/bmp280"

int main() {
    int fd;
    char buffer[20];
    ssize_t bytesRead;

    // Open the device file for reading
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    // Read the temperature value from the device
    bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead < 0) {
        perror("Failed to read the device");
        close(fd);
        return -1;
    }

    buffer[bytesRead] = '\0';
    printf("Temperature in Degree Celsius: %s \n", buffer);

    
    close(fd);

    return 0;
}
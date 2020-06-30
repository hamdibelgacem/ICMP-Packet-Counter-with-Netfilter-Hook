#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "../dev/icmpcount.h"

#define ICMPCOUNT_DRIVER_NAME "/dev/icmpcount"

static bool running = true;

void exit_handler(int sig_num)
{
	(void)sig_num;
	running = false;
}

int open_driver(const char* driver_name) {

    printf("* Open Driver\n");

    int fd_driver = open(driver_name, O_RDWR);
    if (fd_driver == -1) {
        printf("ERROR: could not open \"%s\".\n", driver_name);
        printf("    errno = %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	return fd_driver;
}

void close_driver(const char* driver_name, int fd_driver) {

    printf("* Close Driver\n");

    int result = close(fd_driver);
    if (result == -1) {
        printf("ERROR: could not close \"%s\".\n", driver_name);
        printf("    errno = %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}


int main(void) {

	int fd_icmpcount = open_driver(ICMPCOUNT_DRIVER_NAME);
	uint32_t count = 0;	// counting of icmp packet.
	
	signal(SIGINT, exit_handler);

	if (ioctl(fd_icmpcount, START_COUNT, &count) < 0)
	{
		perror("Error on ioctl cmd");
		exit(EXIT_FAILURE);
	}

	while(running)
	{
		sleep(1);
		if (ioctl(fd_icmpcount, GET_COUNT, &count) < 0)
		{
			perror("Error on ioctl cmd");
			exit(EXIT_FAILURE);
		}
		printf("Number of ICMP packet hooked : %u\n", count);		
	}

	if (ioctl(fd_icmpcount, STOP_COUNT, &count) < 0)
	{
		perror("Error on ioctl cmd");
		exit(EXIT_FAILURE);
	}
		
	close_driver(ICMPCOUNT_DRIVER_NAME, fd_icmpcount);

	return EXIT_SUCCESS;
}



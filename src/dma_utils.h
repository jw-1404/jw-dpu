#pragma  once

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

uint64_t getopt_integer(char *optarg);

ssize_t read_to_buffer(char *fname, int fd, char *buffer, uint64_t size, uint64_t base);

ssize_t write_from_buffer(char *fname, int fd, char *buffer, uint64_t size, uint64_t base);

ssize_t aio_read_to_buffer(char *devname, int fd, char *buffer, uint64_t size);

/* ssize_t aio_write_from_buffer(char *fname, int fd, char *buffer, uint64_t size, uint64_t base); */

void timespec_sub(struct timespec *t1, struct timespec *t2);

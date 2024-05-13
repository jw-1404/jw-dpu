#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <libaio.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

uint64_t getopt_integer(char *optarg);

ssize_t read_to_buffer(char *fname, int fd, char *buffer, uint64_t size,
                       uint64_t base);

ssize_t write_from_buffer(char *fname, int fd, char *buffer, uint64_t size,
                          uint64_t base);

ssize_t aio_read_to_buffer(char *devname, int fd, char *buffer, uint64_t size);

ssize_t aio_write_from_buffer(char *fname, int fd, char *buffer, uint64_t size);

ssize_t libaio_read_to_buffer(char *devname, int fd, char *buffer, uint64_t size);

ssize_t libaio_write_from_buffer(char *fname, int fd, char *buffer, uint64_t size);

void timespec_sub(struct timespec *t1, struct timespec *t2);

#ifdef __cplusplus
}
#endif

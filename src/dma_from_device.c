/*
 * This file is part of the Xilinx DMA IP Core driver tool for Linux
 *
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under BSD-style license (found in the
 * LICENSE file in the root directory of this source tree)
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "dma_utils.h"

#define DEVICE_NAME_DEFAULT "/dev/xdma0_c2h_0"
#define SIZE_DEFAULT (4096)
#define COUNT_DEFAULT (1)

int verbose = 0;

static struct option const long_opts[] = {
	{"device", required_argument, NULL, 'd'},
	{"address", required_argument, NULL, 'a'},
	{"size", required_argument, NULL, 's'},
	{"offset", required_argument, NULL, 'o'},
	{"count", required_argument, NULL, 'c'},
	{"file", required_argument, NULL, 'f'},
	{"eop_flush", no_argument, NULL, 'e'},
	{"help", no_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

static int test_dma(char *devname, uint64_t addr, 
		uint64_t size, uint64_t offset, uint64_t count,
                    char *ofname, uint32_t);
static int eop_flush = 0;

static void usage(const char *name)
{
	int i = 0;
	fprintf(stdout, "%s\n\n", name);
	fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);
	fprintf(stdout, "Read via SGDMA, optionally save output to a file\n\n");

	fprintf(stdout, "  -%c (--%s) device (defaults to %s)\n",
		long_opts[i].val, long_opts[i].name, DEVICE_NAME_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) the start address on the AXI bus\n",
	       long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout,
		"  -%c (--%s) size of a single transfer in bytes, default %d.\n",
		long_opts[i].val, long_opts[i].name, SIZE_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) page offset of transfer\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) number of transfers, default is %d.\n",
	       long_opts[i].val, long_opts[i].name, COUNT_DEFAULT);
	i++;
	fprintf(stdout,
		"  -%c (--%s) file to write the data of the transfers\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout,
		 "  -%c (--%s) end dma when ST end-of-packet(eop) is rcved\n",
		long_opts[i].val, long_opts[i].name);
	fprintf(stdout,
		 "\t\t* streaming only, ignored for memory-mapped channels\n");
	fprintf(stdout,
		 "\t\t* acutal # of bytes dma'ed could be smaller than specified\n");
	i++;
	fprintf(stdout, "  -%c (--%s) print usage help and exit\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) verbose output\n",
		long_opts[i].val, long_opts[i].name);
	i++;

	fprintf(stdout, "\nReturn code:\n");
	fprintf(stdout, "  0: all bytes were dma'ed successfully\n");
	fprintf(stdout, "     * with -e set, the bytes dma'ed could be smaller\n");
	fprintf(stdout, "  < 0: error\n\n");
}

int main(int argc, char *argv[])
{
	int cmd_opt;
	char *device = DEVICE_NAME_DEFAULT;
	uint64_t address = 0;
	uint64_t size = SIZE_DEFAULT;
	uint64_t offset = 0;
	uint64_t count = COUNT_DEFAULT;
  uint32_t wait_us = 0;
	char *ofname = NULL;

	while ((cmd_opt = getopt_long(argc, argv, "vhec:f:d:a:k:s:o:u:", long_opts,
			    NULL)) != -1) {
		switch (cmd_opt) {
		case 0:
			/* long option */
			break;
		case 'd':
			/* device node name */
			device = strdup(optarg);
			break;
		case 'a':
			/* RAM address on the AXI bus in bytes */
			address = getopt_integer(optarg);
			break;
		case 's':
			/* RAM size in bytes */
			size = getopt_integer(optarg);
			break;
		case 'o':
			offset = getopt_integer(optarg) & 4095;
			break;
			/* count */
		case 'c':
			count = getopt_integer(optarg);
			break;
			/* count */
		case 'f':
			ofname = strdup(optarg);
			break;
			/* print usage help and exit */
		case 'v':
			verbose = 1;
			break;
		case 'e':
			eop_flush = 1;
			break;
		case 'u':
			wait_us = getopt_integer(optarg);
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(0);
			break;
		}
	}
	if (verbose)
	fprintf(stdout,
		"dev %s, addr 0x%lx, size 0x%lx, offset 0x%lx, "
		"count %lu\n",
		device, address, size, offset, count);

	return test_dma(device, address, size, offset, count, ofname, wait_us);
}

static int test_dma(char *devname, uint64_t addr,
			uint64_t size, uint64_t offset, uint64_t count,
                    char *ofname, uint32_t wait_us)
{
	ssize_t rc = 0;
	size_t out_offset = 0;
	size_t bytes_done = 0;
	uint64_t i;
	char *buffer = NULL;
	char *allocated = NULL;
	struct timespec ts_start, ts_end;
	int out_fd = -1;
	int fpga_fd;
	long total_time = 0;
	float result;
	float avg_time = 0;
	int underflow = 0;

	/*
	 * use O_TRUNC to indicate to the driver to flush the data up based on
	 * EOP (end-of-packet), streaming mode only
	 */
	if (eop_flush)
		fpga_fd = open(devname, O_RDWR | O_TRUNC);
	else
		fpga_fd = open(devname, O_RDWR);

	if (fpga_fd < 0) {
                fprintf(stderr, "unable to open device %s, %d.\n",
                        devname, fpga_fd);
                perror("open device");
                return -EINVAL;
  }

	/* create file to write data to */
	if (ofname) {
		out_fd = open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_SYNC,
				0666);
		if (out_fd < 0) {
      fprintf(stderr, "unable to open output file %s, %d.\n",
              ofname, out_fd);
      perror("open output file");
      rc = -EINVAL;
      goto out;
    }
	}

	posix_memalign((void **)&allocated, 4096 /*alignment */ , size + 4096);
	if (!allocated) {
		fprintf(stderr, "OOM %lu.\n", size + 4096);
		rc = -ENOMEM;
		goto out;
	}

	buffer = allocated + offset;
	if (verbose)
	fprintf(stdout, "host buffer 0x%lx, %p.\n", size + 4096, buffer);

	for (i = 0; i < count; i++) {
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    //
    uint64_t bytes_done = 0;
    char* buf=buffer;
    int loop = 0;

    while(bytes_done < size) {
      uint64_t bytes = size - bytes_done;

      rc = read_to_buffer(devname, fpga_fd, buf, bytes, addr);
      if (rc < 0) { // ignore the any error and continue 
        /* goto out; */
        fprintf(stderr, "%s: wait new data ...\n", devname);
        continue;
    }

      if (rc != bytes) { // underflow is not error
        fprintf(stderr, "%s (loop-%d), read underflow 0x%lx/0x%lx @ 0x%lx.\n",
                devname, loop, rc, bytes, offset);
      }

      bytes_done += rc;
      buf +=rc;
      loop++;
    }

    fprintf(stdout, "%s (loop-%d, the end), read 0x%lx/0x%lx.\n",
            devname, loop, bytes_done, size);

		clock_gettime(CLOCK_MONOTONIC, &ts_end);

		/* subtract the start time from the end time */
		timespec_sub(&ts_end, &ts_start);
		total_time += ts_end.tv_nsec;

		/* a bit less accurate but side-effects are accounted for */
		if (verbose)
      fprintf(stdout,
              "#%lu: CLOCK_MONOTONIC %ld.%09ld sec. read %ld/%ld bytes\n",
              i, ts_end.tv_sec, ts_end.tv_nsec, bytes_done, size);

		/* file argument given? */
		if (out_fd >= 0) {
			rc = write_from_buffer(ofname, out_fd, buffer,
					 bytes_done, out_offset);
			if (rc < 0 || rc < bytes_done)
				goto out;
			/* out_offset += bytes_done; */
		}

    //
    if(wait_us) usleep(wait_us);
	}

	if (!underflow) {
		avg_time = (float)total_time/(float)count;
		result = ((float)size)*1000/avg_time;
		if (verbose)
			printf("** Avg time device %s, total time %ld nsec, avg_time = %f, size = %lu, BW = %f \n",
				devname, total_time, avg_time, size, result);
		printf("%s ** Average BW = %lu, %f\n", devname, size, result);
		rc = 0;
	} else if (eop_flush) {
		/* allow underflow with -e option */
		rc = 0;
	} else 
		rc = -EIO;

out:
	close(fpga_fd);
	if (out_fd >= 0)
		close(out_fd);
	free(allocated);

	return rc;
}

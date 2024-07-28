#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/types.h>
#include "dma_utils.h"

/* ltoh: little endian to host */
/* htol: host to little endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ltohl(x)       (x)
#define ltohs(x)       (x)
#define htoll(x)       (x)
#define htols(x)       (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ltohl(x)     __bswap_32(x)
#define ltohs(x)     __bswap_16(x)
#define htoll(x)     __bswap_32(x)
#define htols(x)     __bswap_16(x)
#endif

void push_to_buffer(uint64_t value, char* buffer)
{
  buffer[0] = value & 0xFF;
  buffer[1] = (value >> 8) & 0xFF;
  buffer[2] = (value >> 16) & 0xFF;
  buffer[3] = (value >> 24) & 0xFF;
  buffer[4] = (value >> 32) & 0xFF;
  buffer[5] = (value >> 40) & 0xFF;
  buffer[6] = (value >> 48) & 0xFF;
  buffer[7] = (value >> 56) & 0xFF;
}

int main(int argc, char **argv)
{
	off_t page_size;
	page_size = sysconf(_SC_PAGESIZE);
  int blk_size=8;

  //
	int fd;
	int err = 0;
	uint64_t read_result, writeval;
	char access_width = 'l';
  uint64_t size = 8;

	/* not enough arguments given? */
	if (argc < 3) {
		fprintf(stderr,
			"\nUsage:\t%s <device> [type [data]]\n"
			"\tdevice  : character device to access\n"
			"\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\n"
			"\tdata    : data to be written for a write\n\n",
			argv[0]);
		exit(1);
	}
	printf("device: %s, access %s.\n", argv[1], argc >= 4 ? "write" : "read");

	if (argc >= 3)
		access_width = tolower(argv[2][0]);
	printf("access width: ");

	if (access_width == 'b') {
    size = 1;
    printf("byte (8-bits)\n");
  }
	else if (access_width == 'h') {
    size = 2;
		printf("half word (16-bits)\n");
  }
	else if (access_width == 'w') {
    size = 4;
		printf("word (32-bits)\n");
  }
	else if (access_width == 'l')
		printf("long (64-bits)\n");
	else {
		printf("default to long (64-bits)\n");
	}

  /* open the device */
	if ((fd = open(argv[1], O_RDWR)) == -1) {
		printf("character device %s opened failed: %s.\n", argv[1], strerror(errno));
		return -errno;
	}
	printf("character device %s opened.\n", argv[1]);

  /* memory alignment */
  char* allocated;
	err = posix_memalign((void **)&allocated, page_size, page_size);
	if (err || !allocated) {
    printf("Error allocating aligned memory\n");
    goto close;
	}

	/* read only */
	if (argc <= 3) {
    /* read until required nr of bytes */
    while (1) {
      err = read_to_buffer(argv[1], fd, allocated, blk_size, 0);
      if (err < 0) { // ignore the any error and continue
        fprintf(stderr, "%s: wait new data ...\n", argv[1]);
        usleep(100);
        continue;
      }
      break;
    }
    
		switch (access_width) {
		case 'b':
			read_result = *((uint8_t *) allocated);
			printf ("Read 8-bits value : 0x%02x\n", (uint64_t)read_result);
			break;
		case 'h':
			read_result = *((uint16_t *) allocated);
			/* swap 16-bit endianess if host is not little-endian */
			/* read_result = ltohs(read_result); */
			printf ("Read 16-bit value: 0x%04x\n", (uint64_t)read_result);
			break;
		case 'w':
			read_result = *((uint32_t *) allocated);
			/* swap 32-bit endianess if host is not little-endian */
			/* read_result = ltohl(read_result); */
			printf ("Read 32-bit value : 0x%08x\n", (uint64_t)read_result);
			break;
		case 'l':
			read_result = *((uint64_t *) allocated);
			/* swap 32-bit endianess if host is not little-endian */
			/* read_result = ltohl(read_result); */
			printf ("Read 32-bit value : 0x%16lx\n", (uint64_t)read_result);
			break;
		default:
			fprintf(stderr, "Illegal data type '%c'.\n", access_width);
			err = 1;
			goto close;
		}

    printf("Read %d bytes (hex): ", blk_size);
    for(int i=0;i<blk_size;i++) {
      printf("%02x ", allocated[i]);
    }
    printf("\n");
	}

	/* data value given, i.e. writing? */
	if (argc >= 4) {
		writeval = strtoul(argv[3], 0, 0);
    push_to_buffer(writeval, allocated);
    err = write_from_buffer(argv[1], fd, allocated, blk_size, 0);
    if (err < 0) {
      fprintf(stderr, "%s: can't write data\n", argv[1]);
      goto close;
    }

		switch (access_width) {
		case 'b':
			printf("Write 8-bits value 0x%02x\n", writeval);
			break;
		case 'h':
			printf("Write 16-bits value 0x%04x\n", writeval);
			/* swap 16-bit endianess if host is not little-endian */
			/* writeval = htols(writeval); */
			break;
		case 'w':
			printf("Write 32-bits value 0x%08x\n", writeval);
			/* swap 32-bit endianess if host is not little-endian */
			/* writeval = htoll(writeval); */
			break;
		case 'l':
			printf("Write 64-bits value 0x%16lx\n", writeval);
      break;
		default:
			fprintf(stderr, "Illegal data type '%c'.\n",
				access_width);
			err = 1;
		}
	}

close:
	close(fd);
  free(allocated);

	return err;
}

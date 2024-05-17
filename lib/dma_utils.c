#include "dma_utils.h"
#include <aio.h>
#include <string.h>
#include <stdlib.h>

/*
 * man 2 write:
 * On Linux, write() (and similar system calls) will transfer at most
 * 	0x7ffff000 (2,147,479,552) bytes, returning the number of bytes
 *	actually transferred.  (This is true on both 32-bit and 64-bit
 *	systems.)
 */

#define RW_MAX_SIZE	0x7ffff000


uint64_t getopt_integer(const char *optarg)
{
	int rc;
	uint64_t value;

	rc = sscanf(optarg, "0x%lx", &value);
	if (rc <= 0)
		rc = sscanf(optarg, "%lu", &value);
	//printf("sscanf() = %d, value = 0x%lx\n", rc, value);

	return value;
}

/* read until all requested bytes back */
ssize_t read_to_buffer(const char *fname, int fd, char *buffer, uint64_t size,
			uint64_t base)
{
  fprintf(stdout, "read %s: 0x%lx@0x%lx.\n", fname, size, buffer);

	ssize_t rc;
	off_t offset = base;
  if (offset) {
    rc = lseek(fd, offset, SEEK_SET);
    if (rc != offset) {
      fprintf(stderr, "%s, seek off 0x%lx != 0x%lx.\n",
              fname, rc, offset);
      perror("seek file");
      return -EIO;
    }
  }

  /* read data from file into memory buffer */
  rc = read(fd, buffer, size);
  if (rc < 0) {
    fprintf(stderr, "%s, read 0x%lx @ 0x%lx failed %ld.\n",
            fname, size, offset, rc);
    perror("read file");
    return -EIO;
  }

  fprintf(stdout, "read %s: 0x%lx/0x%lx.\n", fname, rc, size);

	return rc;
}

/* write until all requested bytes out */
ssize_t write_from_buffer(const char *fname, int fd, char *buffer, uint64_t size,
			uint64_t base)
{
	ssize_t rc;
	uint64_t count = 0;
	char *buf = buffer;
	off_t offset = base;
	int loop = 0;

	while (count < size) {
		uint64_t bytes = size - count;

		if (bytes > RW_MAX_SIZE)
			bytes = RW_MAX_SIZE;

		if (offset) {
			rc = lseek(fd, offset, SEEK_SET);
			if (rc != offset) {
				fprintf(stderr, "%s, seek off 0x%lx != 0x%lx.\n",
					fname, rc, offset);
				perror("seek file");
				return -EIO;
			}
		}

		/* write data to file from memory buffer */
		rc = write(fd, buf, bytes);
		if (rc < 0) {
			fprintf(stderr, "%s, write 0x%lx @ 0x%lx failed %ld.\n",
				fname, bytes, offset, rc);
			perror("write file");
			return -EIO;
		}

		if (rc != bytes) {
			fprintf(stderr, "%s (loop-%d), write underflow 0x%lx/0x%lx @ 0x%lx.\n",
              fname, loop, rc, bytes, offset);
		}

		count += rc;
		buf += rc;
		loop++;
	}

  fprintf(stdout, "write %s (final): 0x%lx/0x%lx.\n",
          fname, count, size);

	return count;
}


/* Subtract timespec t2 from t1
 *
 * Both t1 and t2 must already be normalized
 * i.e. 0 <= nsec < 1000000000
 */
static int timespec_check(struct timespec *t)
{
	if ((t->tv_nsec < 0) || (t->tv_nsec >= 1000000000))
		return -1;
	return 0;

}

void timespec_sub(struct timespec *t1, struct timespec *t2)
{
	if (timespec_check(t1) < 0) {
		fprintf(stderr, "invalid time #1: %lld.%.9ld.\n",
			(long long)t1->tv_sec, t1->tv_nsec);
		return;
	}
	if (timespec_check(t2) < 0) {
		fprintf(stderr, "invalid time #2: %lld.%.9ld.\n",
			(long long)t2->tv_sec, t2->tv_nsec);
		return;
	}
	t1->tv_sec -= t2->tv_sec;
	t1->tv_nsec -= t2->tv_nsec;
	if (t1->tv_nsec >= 1000000000) {
		t1->tv_sec++;
		t1->tv_nsec -= 1000000000;
	} else if (t1->tv_nsec < 0) {
		t1->tv_sec--;
		t1->tv_nsec += 1000000000;
	}
}

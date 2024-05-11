#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <aio.h>

#define TNAME "aio_read/5-1.c"

int main() {
  char tmpfname[256];
  #define BUF_SIZE 111
  unsigned char buf[BUF_SIZE];
  unsigned char check[BUF_SIZE];
  int fd;
  struct aiocb aiocb;
  int i;

  snprintf(tmpfname, sizeof(tmpfname), "pts_aio_read_5_1_%d", getpid());
  unlink(tmpfname);
  fd = open(tmpfname, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    printf(TNAME " Error at open(): %s\n", strerror(errno));
    exit(1);
  }

  unlink(tmpfname);

  for (i = 0; i < BUF_SIZE; i++)
    buf[i] = i;

  if (write(fd, buf, BUF_SIZE) != BUF_SIZE) {
    printf(TNAME " Error at write(): %s\n",
           strerror(errno));
    exit(1);
  }

  memset(check, 0xaa, BUF_SIZE);
  memset(&aiocb, 0, sizeof(struct aiocb));
  aiocb.aio_fildes = fd;
  aiocb.aio_buf = check;
  aiocb.aio_nbytes = BUF_SIZE;
  aiocb.aio_lio_opcode = LIO_WRITE;

  if (aio_read(&aiocb) == -1) {
    printf(TNAME " Error at aio_read(): %s\n",
           strerror(errno));
    exit(2);
  }

  int err;
  int ret;

  /* Wait until end of transaction */
  while ((err = aio_error (&aiocb)) == EINPROGRESS);

  err = aio_error(&aiocb);
  ret = aio_return(&aiocb);

  if (err != 0) {
    printf(TNAME " Error at aio_error() : %s\n", strerror (err));
    close(fd);
    exit(2);
  }

  if (ret != BUF_SIZE) {
    printf(TNAME " Error at aio_return()\n");
    close(fd);
    exit(2);
  }

  /* check it */

  for (i = 0; i < BUF_SIZE; i++) {
    if (buf[i] != check[i])
    {
      printf(TNAME " read values are corrupted\n");
      exit(2);
    }
  }

  close(fd);
  printf ("Test PASSED\n");
  return 0;
}

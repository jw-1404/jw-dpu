/*
 * Simplistic version of copy command using async i/o (from libaio man)
 *
 * From:	Stephen Hemminger <shemminger@osdl.org>
 * Copy file by using a async I/O state machine.
 * 1. Start read request
 * 2. When read completes turn it into a write request
 * 3. When write completes decrement counter and free resources
 *
 *
 * Usage: aiocp file(s) desination
 */

#include <unistd.h> // (posix header)
#include <sys/types.h> // (posix header)
#include <stdio.h> // (glibc)
#include <sys/stat.h> // for fstat (glibc)
#include <sys/param.h> // for howmany macro (glibc)
#include <fcntl.h>
#include <errno.h>

#include <libaio.h>

#include <boost/program_options.hpp>
#include <iostream>
#include <string>

namespace po = boost::program_options;

#define AIO_BLKSIZE	(64*1024)
#define AIO_MAXIO	32

static int busy = 0;		// # of I/O's in flight
static int tocopy = 0;		// # of blocks left to copy
static int dstfd = -1;		// destination file descriptor
static const char *dstname = NULL;
static const char *srcname = NULL;

/* io wait */
int io_wait(io_context_t ctx, struct timespec *timeout)
{
	return io_getevents(ctx, 0, 0, NULL, timeout);
}

/* fork of libaio's io_queue_run with verbose printing */
int io_run(io_context_t ctx, bool verbose=false)
{
	static struct timespec timeout = { 0, 0 };
	struct io_event event;
	int ret;

  if(verbose) std::cout << "in io_run\n";
  
	/* FIXME: batch requests? */
	while (1 == (ret = io_getevents(ctx, 0, 1, &event, &timeout))) {
		io_callback_t cb = (io_callback_t)event.data;
		struct iocb *iocb = event.obj;

		cb(ctx, iocb, event.res, event.res2);
	}

  if(verbose) std::cout << "out io_run\n";

	return ret;
}

/* Fatal error handler */
static void io_error(const char *func, int rc)
{
  if (rc == -ENOSYS)
    fprintf(stderr, "AIO not in this kernel\n");
  else if (rc < 0 && -rc < sys_nerr)
    fprintf(stderr, "%s: %s\n", func, sys_errlist[-rc]);
  else
    fprintf(stderr, "%s: error %d\n", func, rc);

  if (dstfd > 0)
    close(dstfd);
  if (dstname)
    unlink(dstname);
  exit(1);
}

/*
 * Write complete callback.
 * Adjust counts and free resources
 */
static void wr_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  if (res2 != 0) {
    io_error("aio write", res2);
  }
  if (res != iocb->u.c.nbytes) {
    fprintf(stderr, "write missed bytes expect %d got %d\n", iocb->u.c.nbytes, res2);
    exit(1);
  }
  --tocopy;
  --busy;
  free(iocb->u.c.buf);

  memset(iocb, 0xff, sizeof(iocb));	// paranoia
  free(iocb);
  write(2, "w", 1);
}

/*
 * Read complete callback.
 * Change read iocb into a write iocb and start it.
 */
static void rd_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  /* library needs accessors to look at iocb? */
  int iosize = iocb->u.c.nbytes;
  char *buf = static_cast<char*>(iocb->u.c.buf);
  off_t offset = iocb->u.c.offset;

  if (res2 != 0)
    io_error("aio read", res2);
  if (res != iosize) {
    fprintf(stderr, "read missing bytes expect %d got %d\n", iocb->u.c.nbytes, res);
    exit(1);
  }


  /* turn read into write */
  io_prep_pwrite(iocb, dstfd, buf, iosize, offset);
  io_set_callback(iocb, wr_done);
  if (1 != (res = io_submit(ctx, 1, &iocb)))
    io_error("io_submit write", res);
  write(2, "r", 1);
}


int main(int argc, char *const *argv)
{
  int srcfd;
  struct stat st;
  off_t length = 0, offset = 0;
  io_context_t myctx;

  //
  std::string infile;
  std::string outfile;
  int aio_max;
  int aio_blksize;
  bool verbose = false;

  po::options_description desc("allowed opitons");
  desc.add_options()
    ("help,h","help message")
    ("verbose,v", po::bool_switch(&verbose), "verbose mode")
    ("max,m", po::value<int>(&aio_max)->default_value(AIO_MAXIO), "max number of aio requests")
    ("size,s", po::value<int>(&aio_blksize)->default_value(AIO_BLKSIZE), "block size of a single aio copy")
    ("input,i", po::value<std::string>(&infile), "input file")
    ("output,o", po::value<std::string>(&outfile), "outfile file");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("input") || !vm.count("output")) {
    std::cout << desc << "\n";
    return 0;
  }

  //
  srcname = infile.c_str();
  if ((srcfd = open(srcname, O_RDONLY)) < 0) {
    perror(srcname);
    exit(1);
  }
  if (fstat(srcfd, &st) < 0) {
    perror("fstat");
    exit(1);
  }
  length = st.st_size;

  dstname = outfile.c_str();
  if ((dstfd = open(dstname, O_WRONLY | O_CREAT, 0666)) < 0) {
    close(srcfd);
    perror(dstname);
    exit(1);
  }

  /* initialize state machine */
  memset(&myctx, 0, sizeof(myctx));
  io_queue_init(aio_max, &myctx);
  tocopy = howmany(length, aio_blksize);

  while (tocopy > 0) {
    int i, rc;
    /* Submit as many reads as once as possible upto aio_max */
    int n = MIN(MIN(aio_max - busy, aio_max / 2),
                howmany(length - offset, aio_blksize));
    if (n > 0) {
	    struct iocb *ioq[n];

	    for (i = 0; i < n; i++) {
        struct iocb *io = (struct iocb *) malloc(sizeof(struct iocb));
        int iosize = MIN(length - offset, aio_blksize);
        char *buf = (char *) malloc(iosize);

        if (NULL == buf || NULL == io) {
          fprintf(stderr, "out of memory\n");
          exit(1);
        }

        io_prep_pread(io, srcfd, buf, iosize, offset);
        io_set_callback(io, rd_done);
        ioq[i] = io;
        offset += iosize;
	    }

	    rc = io_submit(myctx, n, ioq);
	    if (rc < 0)
        io_error("io_submit", rc);

	    busy += n;
    }

    // Handle IO's that have completed
    rc = io_queue_run(myctx);
    // rc = io_run(myctx,verbose);
    if (rc < 0)
      io_error("io_queue_run", rc);

    // if we have maximum number of i/o's in flight
    // then wait for one to complete
    if (busy == aio_max) {
      if(verbose)
        std::cout << "max io nr reached, passive waiting for available io slot...\n";

      rc = io_wait(myctx, NULL);
      // rc = io_getevents(myctx, 0, 0, NULL, NULL);
      if (rc < 0)
        io_error("io_queue_wait", rc);
    }

    if(verbose)
      sleep(1);
  }

  close(srcfd);
  close(dstfd);
  exit(0);
}

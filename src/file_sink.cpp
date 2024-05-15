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
#define AIO_MAXIO	1

static char *buf = NULL;
static off_t length = 0;
static off_t offset = 0;
static bool busy = false;		// # of I/O's in flight
static int srcfd = -1;		// destination file descriptor
static int dstfd = -1;		// destination file descriptor
static const char *dstname = NULL;
static const char *srcname = NULL;

/* io wait */
int io_wait(io_context_t ctx, struct timespec *timeout)
{
	return io_getevents(ctx, 0, 0, NULL, timeout);
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

  if (srcfd > 0)
    close(srcfd);

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
    fprintf(stderr, "write missed bytes expect %d got %d\n", iocb->u.c.nbytes, res);
    exit(1);
  }
  offset += res;
  length -= res;
  // memset(iocb, 0xff, sizeof(iocb));	// paranoia
  free(iocb);

  busy=false;
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
  // off_t offset = iocb->u.c.offset;

  if (res2 != 0)
    io_error("aio read", res2);
  if (res != iosize) {
    fprintf(stderr, "read missing bytes expect %d got %d\n", iosize, res);
    // exit(1);
  }

  /* turn read into write */
  io_prep_pwrite(iocb, dstfd, buf, res, offset);
  io_set_callback(iocb, wr_done);
  if (1 != (res = io_submit(ctx, 1, &iocb)))
    io_error("io_submit write", res);
  write(2, "r", 1);
}


/* main */
int main(int argc, char* argv[])
{
  long page_size = sysconf(_SC_PAGESIZE);

  // args config
  io_context_t myctx;

  std::string outfile;
  int aio_max;
  int aio_blksize;
  bool verbose = false;

  po::options_description desc("allowed opitons");
  desc.add_options()
    ("help,h","help message")
    ("verbose,v", po::bool_switch(&verbose), "verbose mode")
    ("length,l", po::value<off_t>(&length)->default_value(0), "total length of reading (in bytes)")
    ("max,m", po::value<int>(&aio_max)->default_value(AIO_MAXIO), "max number of aio requests")
    ("size,s", po::value<int>(&aio_blksize)->default_value(AIO_BLKSIZE), "block size of a single aio copy")
    ("output,o", po::value<std::string>(&outfile), "outfile file");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("output")) {
    std::cout << desc << "\n";
    return 0;
  }

  // output init
  dstname = outfile.c_str();
  if ((dstfd = open(dstname, O_WRONLY | O_CREAT, 0666)) < 0) {
    perror(dstname);
    exit(1);
  }

  // dpu init
  srcname = "/dev/xdma0_c2h_0";
  if ((srcfd = open(srcname, O_RDONLY)) < 0) {
    perror(srcname);
    close(dstfd);
    exit(1);
  }

  // buffer init
  posix_memalign((void **)&buf, page_size, aio_blksize + page_size);
  if (!buf) {
    perror("can't allocate memory");
    close(srcfd);
    close(dstfd);
    exit(1);
  }

  // til input eof
  memset(&myctx, 0, sizeof(myctx));
  io_queue_init(aio_max, &myctx);

  while(length >0) {
    int rc;
    int n = howmany(length, aio_blksize);
    if (!busy && n > 0) {
	    struct iocb *ioq[1];
      struct iocb *io = (struct iocb*) malloc(sizeof(struct iocb));
      int iosize = MIN(length, aio_blksize);
      if (NULL == io) {
        fprintf(stderr, "out of memory\n");
        exit(1);
      }

      io_prep_pread(io, srcfd, buf, iosize, 0);
      io_set_callback(io, rd_done);
      ioq[0] = io;

      rc = io_submit(myctx, 1, ioq);
      if (rc < 0)
        io_error("io_submit", rc);

      busy = true;
    }
    
    // Handle IO's that have completed
    rc = io_queue_run(myctx);
    if (rc < 0)
      io_error("io_queue_run", rc);

    // wait
    // if (busy) {
    //   if(verbose)
    //     std::cout << "max io nr reached, passive waiting for available io slot...\n";

    //   rc = io_wait(myctx, NULL);
    //   if (rc < 0)
    //     io_error("io_queue_wait", rc);
    // }

    //
    if(busy) {
      usleep(1000);
    }
  }

  // 
  close(srcfd);
  close(dstfd);
  free(buf);
  
  exit(0);
}

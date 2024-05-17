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
#define AIO_MAXWAIT 10000

static bool verbose = false;
static int sleeped = 0;
static bool first_sleeped = false;
static char *buf = NULL;
static int64_t length = 0;
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
  // if (dstname)
  //   unlink(dstname);

  exit(1);
}

/*
 * Write complete callback.
 * Adjust counts and free resources
 */
static void wr_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  if(verbose) std::cout << "in wr_done\n";

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
  // free(iocb);

  busy=false;
  sleeped = 0;

  if(verbose)
    std::cout <<"wd_done: current:" << res << " , total: " << offset << " bytes received\n";
  write(2, "w\n", 2);
}

/*
 * Read complete callback.
 * Change read iocb into a write iocb and start it.
 */
static void rd_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  if(verbose) std::cout << "in rd_done\n";
  
  /* library needs accessors to look at iocb? */
  int iosize = iocb->u.c.nbytes;
  char *buf = static_cast<char*>(iocb->u.c.buf);
  // off_t offset = iocb->u.c.offset;

  if (res2 != 0)
    io_error("aio read", res2);
  if (res != iosize) {
    fprintf(stderr, "read unexpected nr of bytes: expect %d, got %d\n", iosize, res);
    // exit(1);
  }

  /* turn read into write */
  if(dstfd > 0) {
    io_prep_pwrite(iocb, dstfd, buf, res, offset);
    io_set_callback(iocb, wr_done);
    if (1 != (res = io_submit(ctx, 1, &iocb)))
      io_error("io_submit write", res);
  }
  else {
    offset += res;
    length -= res;
    // memset(iocb, 0xff, sizeof(iocb));	// paranoia
    // free(iocb);

    busy=false;
    sleeped = 0;
    if(verbose)
      std::cout <<"rd_done: current:" << res << " , total: " << offset << " bytes received\n";

  }

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
  int aio_wait;
  bool eop_flush = false;

  po::options_description desc("allowed opitons");
  desc.add_options()
    ("help,h","help message")
    ("verbose,v", po::bool_switch(&verbose), "verbose mode")
    ("eopflush,e", po::bool_switch(&eop_flush), "End-of-Packet flush of XDMA")
    ("length,l", po::value<int64_t>(&length)->default_value(0), "total length of reading (in bytes)")
    ("max,m", po::value<int>(&aio_max)->default_value(AIO_MAXIO), "max number of aio requests")
    ("size,s", po::value<int>(&aio_blksize)->default_value(AIO_BLKSIZE), "block size of a single aio copy")
    ("wait,w", po::value<int>(&aio_wait)->default_value(AIO_MAXWAIT), "max wait time (s) without new data from xdma")
    ("output,o", po::value<std::string>(&outfile), "outfile file");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

  // output init
  if(vm.count("output")) {
    dstname = outfile.c_str();
    if ((dstfd = open(dstname, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666)) < 0) {
      perror(dstname);
      exit(1);
    }
  }

  // dpu init
  srcname = "/dev/xdma0_c2h_0";
  if(!eop_flush)
    srcfd = open(srcname, O_RDONLY);
  else
    srcfd = open(srcname, O_RDONLY | O_TRUNC);

  if (srcfd < 0) {
    perror(srcname);
    if(dstfd > 0) close(dstfd);
    exit(1);
  }

  // buffer init
  posix_memalign((void **)&buf, page_size, aio_blksize + page_size);
  if (!buf) {
    perror("can't allocate memory");
    close(srcfd);
    if(dstfd > 0) close(dstfd);
    exit(1);
  }

  // til input eof
  memset(&myctx, 0, sizeof(myctx));
  io_queue_init(aio_max, &myctx);

  struct iocb *io = (struct iocb*) malloc(sizeof(struct iocb));
  if (NULL == io) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  while(length >0) {
    int rc;
    int n = howmany(length, aio_blksize);

    // if(verbose) {
    //   std::cout << "remaining bytes to read: " << length  << ", busy: " << busy << " , n=" << n<< "\n";
    // }

    if (!busy && n > 0) {
      int iosize = MIN(length, aio_blksize);
      io_prep_pread(io, srcfd, buf, iosize, 0);
      io_set_callback(io, rd_done);
      rc = io_submit(myctx, 1, &io);
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
      sleeped += 1;
    }

    //
    if(sleeped > aio_wait) {
      if(verbose && first_sleeped && sleeped%10000==0) {
        std::cout << "wainted " <<sleeped << " ms\n";
      }
      
      if(!first_sleeped) {
        // struct io_event event;
        // rc = io_cancel(myctx, io, &event);

        if(dstfd > 0)
          close(dstfd);
        dstfd=-1;

        std::cout << "WARN:\n \tmax wait time reached\n"
                  << "\tdata file closed, totally " << offset << " bytes saved.\n"
                  << "\tand future arrived data will be dumped\n";

        first_sleeped = true;
        // sleeped = 0;
        // busy = false;
      }
    }
  }
  std::cout <<"app: end reading\n";

  // 
  close(srcfd);
  if(dstfd > 0)
    close(dstfd);
  std::cout <<"app: all closed\n";
  free(buf);
  std::cout <<"app: eol\n";
  
  exit(0);
}

#include <sys/types.h>
// #include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
// #include <stdlib.h>
#include <fcntl.h>
// #include <unistd.h>
#include <errno.h>


#include <iostream>
#include <algorithm>
#include <boost/program_options.hpp>
#include <string>


#define DEVICE_NAME_DEFAULT "/dev/xdma0_c2h_0"
#define BLKSIZE_DEFAULT 4096
#define LENGTH_DEFAULT 4096

namespace po = boost::program_options;

static bool verbose = false;
static bool eop_flush = false;
static bool daemon_flag = false;
static int srcfd = -1;		// destination file descriptor
static int dstfd = -1;		// destination file descriptor
static const char *dstname = NULL;
static const char *srcname = NULL;
static std::string infile, outfile;
static char *allocated = NULL;
static uint64_t size = BLKSIZE_DEFAULT;
static uint64_t length = LENGTH_DEFAULT;
static uint64_t total_length = 0;

//
volatile sig_atomic_t keepRunning = 1;
void sigHandler(int sig) {
  keepRunning = 0;
  std::cout << "\nWARN: transfer to be cloesd, waiting for last timeout ...\n";
}

/* Fatal error handler */
void cleanup(const char *func, int rc)
{
  if(rc<0)
    perror(func);
  else
    std::cout << func << std::endl;
  
  if (srcfd > 0)
    close(srcfd);

  if (dstfd > 0)
    close(dstfd);

  if(allocated)
    free(allocated);
  
  std::cout << "Total: " << total_length << " bytes read\n";
  exit(0);
}

/* read from xdma */
ssize_t read_to_buffer(const char *fname, int fd, char *buffer, uint64_t size)
{
  fprintf(stdout, "read %s: 0x%lx@0x%lx.\n", fname, size, buffer);

	ssize_t rc;
  rc = read(fd, buffer, size);
  if (rc < 0) {
    perror("read file");
    return -EIO;
  }

  fprintf(stdout, "read %s: 0x%lx/0x%lx.\n", fname, rc, size);

	return rc;
}


int main(int argc, char *argv[])
{
  long page_size = sysconf(_SC_PAGESIZE);

  //
  signal(SIGINT, sigHandler);

  //
  po::options_description desc("allowed opitons");
  desc.add_options()
    ("help,h","help message")
    ("verbose,v", po::bool_switch(&verbose), "verbose mode")
    ("eopflush,e", po::bool_switch(&eop_flush), "End-of-Packet flush of XDMA")
    ("daemon_flag,d", po::bool_switch(&daemon_flag), "As daemon_flag servic")
    ("length,l", po::value<uint64_t>(&length)->default_value(LENGTH_DEFAULT), "total length of reading (in bytes)")
    ("size,s", po::value<uint64_t>(&size)->default_value(BLKSIZE_DEFAULT), "block size of a single dma request")
    ("input,i", po::value<std::string>(&infile)->default_value(DEVICE_NAME_DEFAULT), "xdma C2H device node")
    ("output,o", po::value<std::string>(&outfile), "name of the file saving data");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

  // output file
  // if(vm.count("output")) {
  //   dstname = outfile.c_str();
  //   if ((dstfd = open(dstname, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666)) < 0) {
  //     perror(dstname);
  //     exit(1);
  //   }
  // }

	/*
	 * xdma device init: use O_TRUNC to indicate to the driver to flush the data up based on
	 * EOP (end-of-packet), streaming mode only
	 */
  srcname = infile.c_str();
  if(!eop_flush)
    srcfd = open(srcname, O_RDONLY);
  else
    srcfd = open(srcname, O_RDONLY | O_TRUNC);
  if (srcfd < 0) {
    perror(srcname);
    if(dstfd > 0) close(dstfd);
    exit(1);
  }

  /* buffer init */
  /* - must aligned with memory page size
   * - one extra page is allocated since may be more data than requested (the transfer unit is 8 bytes)
   */
	int err = posix_memalign((void **)&allocated, page_size, size + page_size);
	if (err || !allocated) {
    std::cout << "Error allocating aligned memory\n";
    if(dstfd > 0) close(dstfd);
    if(srcfd > 0) close(srcfd);
	}

  //
  uint64_t bytes_remaining = daemon_flag ? size : length;

	if(verbose) {
    std::cout << "page-size: " << page_size << ", ";
    std::cout << "allocated address: " << std::hex << (void*)allocated << std::dec <<", ";
    std::cout << "dev: " << infile << ", ";
    std::cout << "blk-size: " << size << ", ";
    if(daemon_flag)
      std::cout << "in daemon mode\n";
    else
      std::cout << "length to read: " << bytes_remaining << "\n";
  }

  uint64_t loop = 0;
	while (keepRunning && bytes_remaining > 0) {
    if(verbose)
      std::cout << "\n" << ++loop << " blk, remaining bytes: " << bytes_remaining  << std::endl;
    
    int bytes_done = 0;
    char* buffer = allocated;
    int iosize = std::min(bytes_remaining, size);

    while(bytes_done < iosize) {
      if(verbose)
        std::cout << "inside one dma blk transfer (" << bytes_done << " / " << iosize << ")" << std::endl;

      uint64_t bytes = iosize - bytes_done;
      int rc = read_to_buffer(srcname, srcfd, buffer, bytes);
      if (rc < 0) { // ignore timeout
        usleep(100);
        fprintf(stderr, "%s: wait new data ...\n", srcname);
        if(keepRunning)
          continue;
        else
          cleanup("Grace exit", 0);
      }

      // if (rc < 0) {
      //   fprintf(stderr, "%s: IO error\n", devname);
      //   goto out;
      // }

      if (rc != bytes) {
        fprintf(stderr, "%s: read underflow 0x%lx/0x%lx.\n",
                srcname, rc, bytes);
      }

      // if (dstfd > 0) {
      //   int erc = write_from_buffer(dstname, dstfd, buffer, rc, 0);
      //   if (erc < 0 || erc < rc) {
      //     cleanup("write outfile", erc);
      //   }
      // }

      //
      bytes_done += rc;
      buffer +=rc;
      total_length +=rc;

      if(!daemon_flag)
        bytes_remaining -= rc;
    }
	}

  cleanup("Normal exit", 0);
}

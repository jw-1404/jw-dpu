#include "dma_utils.h"
#include <signal.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <libaio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#define FATAL(...)                                                             \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    assert(0);                                                                 \
    exit(-1);                                                                  \
  } while (0)

static const void handle_error(int err) {
#define DECL_ERR(X)                                                            \
  case -X:                                                                     \
    FATAL("Error " #X "\n");                                                   \
    break;
  switch (err) {
    DECL_ERR(EFAULT);
    DECL_ERR(EINVAL);
    DECL_ERR(ENOSYS);
    DECL_ERR(EAGAIN);
  };
  if (err < 0)
    FATAL("Unknown error");
#undef DECL_ERR
}

#define IO_RUN(F, ...)                                                         \
  do {                                                                         \
    int err = F(__VA_ARGS__);                                                  \
    handle_error(err);                                                         \
  } while (0)

// 
#include <iostream>
#include <string>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

#define DEVICE_NAME_DEFAULT "/dev/xdma0_h2c_0"
#define SIZE_DEFAULT 1
#define COUNT_DEFAULT 1
#define FILENAME_DEFAULT "output_backup.dat"

int dpu_fd = -1;
int in_fd = -1;
int out_fd = -1;
char *allocated = NULL;
uint64_t size;

//
void create_rdm_file(const char *filename, int count) {
  FILE *file = fopen(filename, "w+");
  if (file == NULL)
    FATAL("Unable to create crap.dat");

  char *src = new char[size];
  for (size_t i = 0; i < count; ++i) {
    for (int j = 0; j < size; j++)
      src[j] = rand();
    fwrite(src, size, 1, file);
  }
  fclose(file);
  delete[] src;
}

// 
volatile sig_atomic_t keepRunning = 1;
void sigHandler(int sig) {
  keepRunning = 0;
  std::cout << "signal received " <<keepRunning << "\n";
}

// only for xdma streaming device
int main(int argc, char *argv[])
{
  //
  signal(SIGINT, sigHandler);

  //
  long page_size = sysconf(_SC_PAGESIZE);

  //
  std::string device;
  uint64_t count;
  boost::optional<std::string> infile;
  std::string outfile;
  bool verbose = false;
  bool flush = false;

  po::options_description desc("Command options");
  desc.add_options()
    ("help,h", "help messages")
    ("verbose,v", po::bool_switch(&verbose), "verbose mode")
    ("device,d", po::value<std::string>(&device)->default_value(DEVICE_NAME_DEFAULT), "name of xdma device node")
    ("size,s", po::value<uint64_t>(&size)->default_value(SIZE_DEFAULT), "size (in 4096 bytes) of a single transfer")
    ("count,c", po::value<uint64_t>(&count)->default_value(COUNT_DEFAULT), "total number of transfers")
    ("output,o", po::value<std::string>(&outfile)->default_value(FILENAME_DEFAULT), "name of output file")
    ("input,i", po::value(&infile), "name of input file (from random if not provided)");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

  //
  size = size * page_size;

  /* Create a file and fill it with random crap */
  if (!infile) create_rdm_file("crap.dat", count);
  std::string filename = !infile ? "crap.dat" : *infile;
  int in_fd = open(filename.c_str(), O_RDONLY);
  if (in_fd < 0)
    FATAL("Error opening input file");

  //
  dpu_fd = open(device.c_str(), O_WRONLY);
  if (dpu_fd < 0) {
    std::cout << "can't open device node: " << device << "\n";
    return -EINVAL;
  }

  //
  out_fd = open(outfile.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
  if (out_fd < 0) {
    std::cout << "unable to open output file: " << outfile << "\n";
    close(dpu_fd);
    close(in_fd);
    if (out_fd >= 0)
      close(out_fd);
  }

  //
  posix_memalign((void **)&allocated, page_size, size);
  if (!allocated) {
    std::cout << "OOM " << size << "\n";
    close(dpu_fd);
    close(in_fd);
    close(out_fd);
  }

  //
  struct timespec ts_start, ts_end;
  long total_time = 0;

  io_context_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  int maxEvents= 10;
  IO_RUN(io_queue_init, maxEvents, &ctx);
  /* This is the read job we asynchronously run */
  iocb *job = (iocb *)new iocb[1];
  struct timespec timeout = {0, 1000000}; // 1 ms

  for (int i = 0; i < count; i++) {
    memset(allocated, 0, size);
    int rc = read_to_buffer("", in_fd, allocated, size, 0);
    if (rc < 0 || rc < size) FATAL("insufficient input bytes\n");
    write_from_buffer("", out_fd, allocated, size, 0);

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    io_prep_pwrite(job, dpu_fd, allocated, size, 0);
    IO_RUN(io_submit, ctx, 1, &job);
    struct io_event evt;
    // int evtnum = io_getevents(ctx, 1, 1, &evt, &timeout);
    // std::cout << evtnum << " events\n";
    // if (evtnum <= 0)
    //   io_cancel(ctx, job, NULL);
    while (!io_getevents(ctx, 1, 1, &evt, &timeout) && keepRunning) {
      std::cout <<"send pending...\n";
    }

    //
    if(!keepRunning) {
      io_cancel(ctx, job, NULL);
      std::cout << "grace exit\n";
      break;
    }

    //
    std::cout << "transfered counts: " << i << "\n";

    /* subtract the start time from the end time */
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);
    total_time += ts_end.tv_nsec;
	}

  float avg_time = (float)total_time/(float)count;
  float result = ((float)size)*1000/avg_time;
  std::cout << device << ": average BW = " << size << ", " << result << "\n";

  //
  // io_queue_release(ctx);
  free(allocated);
  close(dpu_fd);
  close(in_fd);
  close(out_fd);
  return 0;
}

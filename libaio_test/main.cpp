#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <libaio.h>
#include <unistd.h>
#include <string>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>

namespace po = boost::program_options;

#define FATAL(...)\
  do {\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
    assert(0);\
    exit(-1);\
  } while (0)

static const void handle_error(int err) {
#define DECL_ERR(X) case -X: FATAL("Error "#X"\n"); break;
  switch (err) {
    DECL_ERR(EFAULT);
    DECL_ERR(EINVAL);
    DECL_ERR(ENOSYS);
    DECL_ERR(EAGAIN);
  };
  if (err < 0) FATAL("Unknown error");
#undef DECL_ERR
}

#define IO_RUN(F, ...)\
  do {\
    int err = F(__VA_ARGS__);\
    handle_error(err);\
  } while (0)

#define MB(X) (X * 1024 * 1024)
#define SZ MB(5)

static const int maxEvents = 10;
char *dst = NULL;   // data we are reading
int fd = -1;        // file to open
FILE* fd_out = NULL;        // file for saving

void create_rdm_file(const char* filename, int count)
{
    FILE *file = fopen(filename, "w+");
    if (file == NULL)
      FATAL("Unable to create crap.dat");

    char* src = new char[SZ];
    for (size_t i = 0; i < count; ++i) {
      for (int j = 0; j < SZ; j++)
        src[j] = rand();
      fwrite(src, SZ, 1, file);
    }
    fclose(file);
    delete[] src;
}

void save(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  if (res2 || res != SZ)
    FATAL("Error in async IO");
  fwrite(dst, SZ, 1, fd_out);
}


int main(int argc, char *argv[]) {
  boost::optional<std::string> infile;
  boost::optional<std::string> outfile;
  int COUNT;

  po::options_description desc("allowed opitons");
  desc.add_options()
    ("help,h","help message")
    ("count,c", po::value<int>(&COUNT)->default_value(10), "number of blocks")
    ("input,i", po::value(&infile), "input file")
    ("output,o", po::value(&outfile), "outfile file");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

  /* Create a file and fill it with random crap */
  if (!infile) create_rdm_file("crap.dat", COUNT);

  /* Prepare the file to read */
  std::string filename = !infile ? "crap.dat" : *infile;
  int fd = open(filename.c_str(), O_NONBLOCK, 0);
  if (fd < 0)
    FATAL("Error opening input file");

  filename = !outfile ? "crap_out.dat" : *outfile;
  fd_out = fopen(filename.c_str(), "w+");
  if (!fd_out)
    FATAL("Error opening output file: %s", filename.c_str());

  /* Now use *real* asynchronous IO to read back the file */
  io_context_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  IO_RUN(io_queue_init, maxEvents, &ctx);
  /* This is the read job we asynchronously run */
  iocb *job = (iocb *)new iocb[1];
  struct timespec timeout = {0, 0};

  dst = new char[SZ];
  for (int i = 0; i < COUNT; i++) {
    memset(dst, 0, SZ);
    io_prep_pread(job, fd, dst, SZ, i * SZ);
    // io_set_callback(job, save);

    /* Issue it now */
    IO_RUN(io_submit, ctx, 1, &job);
    /* Wait for it */
    struct io_event evt;
    // IO_RUN (io_getevents, ctx, 1, 1, &evt, &timeout);
    IO_RUN(io_getevents, ctx, 1, 1, &evt, NULL);
    // IO_RUN(io_getevents, ctx, 0, 0, NULL, &timeout);
    save(ctx, evt.obj, evt.res, evt.res2);
    // fwrite(dst, SZ, 1, fd_out);
  }
    printf("DONE\n");

    close(fd);
    fclose(fd_out);
    delete[] dst;
    io_destroy(ctx);
    return 0;
  }

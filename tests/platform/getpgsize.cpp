#include <unistd.h>
#include <iostream>

int main(int argc, const char **argv) {
  long sz = sysconf(_SC_PAGESIZE);
  std::cout << "page size: " << sz << "\n";
}


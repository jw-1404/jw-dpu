#include <csignal>
#include <iostream>

namespace {
static volatile std::sig_atomic_t gSignalStatus = 1;
}

void signal_handler(int signal) { gSignalStatus = signal; }

int main() {
  // Install a signal handler
  std::signal(SIGINT, signal_handler);

  std::cout << "SignalValue: " << gSignalStatus << '\n';
  std::cout << "Sending signal: " << SIGINT << '\n';
  std::raise(SIGINT);
  std::cout << "SignalValue: " << gSignalStatus << '\n';
}

// Verifies cancellation specifically during the new external deduplication
// phase.
#include "core.h"
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  mt::Config c;
  c.inputs = {argv[1]};
  c.output = argv[2];
  c.overwrite = true;
  c.threads = 6;
  c.memoryMiB = 1;
  c.minGames = 1;
  mt::Progress p;
  p.running = true;
  bool observed = false;
  std::thread monitor([&] {
    while (p.running) {
      {
        std::lock_guard<std::mutex> lock(p.mutex);
        if (p.phase.rfind("Deduplicating complete games", 0) == 0) {
          observed = true;
          p.stop = true;
          return;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  mt::build(c, p);
  monitor.join();
  if (!observed || p.error != "Stopped")
    return 1;
  std::cout << "Stopped during deduplication\n";
}

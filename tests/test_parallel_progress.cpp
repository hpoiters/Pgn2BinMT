// Monitor actual parallel reducers and bounded phase accounting.
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
  c.threads = 18;
  c.memoryMiB = 64;
  c.minGames = 1;
  mt::Progress p;
  p.running = true;
  unsigned peak = 0;
  bool valid = true, partial = false;
  std::thread monitor([&] {
    while (p.running) {
      {
        std::lock_guard<std::mutex> lock(p.mutex);
        unsigned active = p.dedupActive;
        if (active > peak)
          peak = active;
        uint64_t done = p.phaseDone, total = p.phaseTotal;
        if (total && done > total)
          valid = false;
        if (p.phase.rfind("Deduplicating complete games", 0) == 0 && done &&
            done < total)
          partial = true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  mt::build(c, p);
  monitor.join();
  if (!p.error.empty() || peak < 2 || !valid || !partial ||
      p.uniqueGames + p.duplicates != p.valid)
    return 1;
  std::cout << "Peak parallel reducers: " << peak
            << "; counters valid; accounting complete\n";
}

// Checks real phase progress on externally sorted complete-game duplicates.
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
  c.threads = 6;
  c.memoryMiB = 1;
  c.minGames = 3;
  mt::Progress p;
  p.running = true;
  bool partialDedup = false, countersValid = true;
  std::thread monitor([&] {
    while (p.running) {
      {
        std::lock_guard<std::mutex> lock(p.mutex);
        auto done = p.phaseDone.load(), total = p.phaseTotal.load();
        if (total && done > total)
          countersValid = false;
        if (p.phase.rfind("Deduplicating complete games", 0) == 0 && done &&
            done < total)
          partialDedup = true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  mt::build(c, p);
  monitor.join();
  if (!p.error.empty() || !partialDedup || !countersValid ||
      p.phase != "Process ended: OK" || p.phaseDone != 1 || p.phaseTotal != 1 ||
      !p.readMilliseconds || p.records != 10 || p.uniqueGames != 3)
    return 1;
  std::cout << "Phase counters valid; intermediate dedup progress observed; "
               "final progress 100%\n";
}

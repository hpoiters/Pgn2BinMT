// Regression: terminal read-rate duration must exist and remain frozen.
#include "core.h"
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc, char **argv) {
  if (argc != 4) return 2;
  mt::initialize();
  mt::Config c; c.inputs={argv[1]}; c.output=argv[2]; c.overwrite=true;
  c.threads=6; c.memoryMiB=1; c.minGames=1;
  mt::Progress p; p.running=true;
  bool later=std::string(argv[3])=="dedup", observed=false;
  uint64_t before=0;
  auto start=std::chrono::steady_clock::now();
  std::thread monitor([&] {
    while(p.running) {
      std::lock_guard<std::mutex> lock(p.mutex);
      bool ready=later ? p.phase.rfind("Deduplicating complete games",0)==0 : p.valid>=1000;
      if(ready) {before=p.readMilliseconds;observed=true;p.stop=true;return;}
      std::this_thread::yield();
    }
  });
  mt::build(c,p); monitor.join();
  auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
  auto frozen=p.readMilliseconds.load();
  if(!observed || p.error!="Stopped" || !frozen || frozen>uint64_t(elapsed+2)) return 1;
  if(later && (!before || frozen!=before)) return 3;
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  if(p.readMilliseconds!=frozen) return 4;
  std::cout << "PASS " << argv[3] << ": timer " << frozen << " ms; frozen after Stop\n";
}

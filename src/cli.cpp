#include "core.h"
#include <csignal>
#include <iostream>
static mt::Progress *current = nullptr;
static void stop(int) {
  if (current)
    current->stop.store(true);
}
int main(int argc, char **argv) {
  mt::Config c;
  bool runFolder = false;
  c.threads = mt::defaultThreads();
  try {
    for (int i = 1; i < argc; i++) {
      std::string s = argv[i];
      auto value = [&]() -> std::string {
        if (++i >= argc)
          throw std::runtime_error("Missing option value");
        return argv[i];
      };
      if (s == "--threads")
        c.threads = std::stoul(value());
      else if (s == "--ply")
        c.ply = std::stoul(value());
      else if (s == "--min-games")
        c.minGames = std::stoul(value());
      else if (s == "--memory-mib")
        c.memoryMiB = std::stoul(value());
      else if (s == "--output")
        c.output = value();
      else if (s == "--run-folder")
        runFolder = true;
      else if (s == "--no-dedup")
        c.deduplicate = false;
      else if (s == "--overwrite")
        c.overwrite = true;
      else
        c.inputs.push_back(s);
    }
    if (runFolder)
      c.output = mt::createRunOutput(c.output);
    mt::Progress p;
    current = &p;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    mt::build(c, p);
    current = nullptr;
    std::cout << p.phase << " read=" << p.read << " valid=" << p.valid
              << " invalid=" << p.invalid << " duplicates=" << p.duplicates
              << " unique=" << p.uniqueGames << " records=" << p.records
              << "\n";
    if (!p.error.empty()) {
      std::cerr << p.error << "\n";
      return 1;
    }
  } catch (const std::exception &e) {
    std::cerr << e.what();
    return 1;
  }
}

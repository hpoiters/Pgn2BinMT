// Pgn2BinMT, GPL-2.0-or-later. See COPYING.
#pragma once
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
namespace mt {
namespace fs = std::filesystem;
struct Config {
  std::vector<fs::path> inputs;
  fs::path output = "Book.bin";
  unsigned threads = 1, ply = 60, minGames = 3;
  size_t memoryMiB = 0;
  bool overwrite = false, deduplicate = true;
};
struct Progress {
  std::atomic<bool> stop{false}, running{false};
  std::atomic<uint64_t> bytes{0}, totalBytes{0}, read{0}, valid{0}, invalid{0},
      events{0}, records{0}, positions{0}, duplicates{0}, uniqueGames{0},
      phaseDone{0}, phaseTotal{0}, readMilliseconds{0}, dedupMilliseconds{0};
  std::atomic<unsigned> active{0}, dedupActive{0};
  std::mutex mutex;
  std::string phase = "Done", error;
  void set(std::string s, uint64_t total = 0) {
    std::lock_guard<std::mutex> g(mutex);
    phaseDone = 0;
    phaseTotal = total;
    phase = std::move(s);
  }
};
void initialize();
unsigned defaultThreads();
size_t automaticMemoryMiB();
uint64_t residentBytes();
fs::path createRunOutput(const fs::path &baseOutput);
void build(const Config &, Progress &);
} // namespace mt

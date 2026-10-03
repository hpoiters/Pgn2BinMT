// Pgn2BinMT, GPL-2.0-or-later. See COPYING.
#include "core.h"
#include "polyglot/attack.h"
#include "polyglot/board.h"
#include "polyglot/fen.h"
#include "polyglot/hash.h"
#include "polyglot/move.h"
#include "polyglot/move_do.h"
#include "polyglot/move_legal.h"
#include "polyglot/option.h"
#include "polyglot/piece.h"
#include "polyglot/san.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <map>
#include <queue>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif
using Clock = std::chrono::steady_clock;
double now_real() {
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}
double now_cpu() { return double(std::clock()) / CLOCKS_PER_SEC; }
namespace mt {
void initialize() {
  static std::once_flag once;
  std::call_once(once, [] {
    util_init();
    option_init();
    square_init();
    piece_init();
    attack_init();
    hash_init();
  });
}
unsigned defaultThreads() {
  return std::max(1u, std::thread::hardware_concurrency() / 2);
}
size_t automaticMemoryMiB() {
#ifdef _WIN32
  MEMORYSTATUSEX m{};
  m.dwLength = sizeof(m);
  GlobalMemoryStatusEx(&m);
  return std::clamp<size_t>(m.ullAvailPhys / 4 / 1048576, 128, 4096);
#else
  return 512;
#endif
}
uint64_t residentBytes() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS m{};
  GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof(m));
  return m.WorkingSetSize;
#else
  std::ifstream f("/proc/self/statm");
  uint64_t v = 0, r = 0;
  f >> v >> r;
  return r * sysconf(_SC_PAGESIZE);
#endif
}
void check(Progress &p) {
  if (p.stop)
    throw std::runtime_error("Stopped");
}
// Disk runs contain events, preserving original source order for PolyGlot's
// order-sensitive 16384-count halving. They are internal files, not BIN files.
struct Event {
  uint64_t key, id;
  uint32_t ply;
  uint16_t move;
  uint8_t score, pad = 0;
};
static_assert(sizeof(Event) == 24);
bool lessEvent(const Event &a, const Event &b) {
  if (a.key != b.key)
    return a.key < b.key;
  if (a.id != b.id)
    return a.id < b.id;
  return a.ply < b.ply;
}
struct Game {
  uint64_t id = 0;
  std::map<std::string, std::string> tags;
  std::vector<std::string> moves;
  std::string error;
};
// Tokenizer bounds tokens and complete games. Comments and RAVs are ignored.
class Lexer {
  std::istream &f;
  Progress &p;
  bool first = true;
  std::streampos anchor = -1;
  uint64_t count = 0;
  int get() {
    int c = f.get();
    if (c != EOF) {
      ++count;
      if (count == 65536) {
        p.bytes += count;
        count = 0;
      }
    }
    return c;
  }

public:
  Lexer(std::istream &s, Progress &q) : f(s), p(q) {}
  ~Lexer() { p.bytes += count; }
  bool recover(std::string &t) {
    f.clear();
    if (anchor != std::streampos(-1))
      f.seekg(anchor);
    else
      return false;
    char buffer[65536];
    while (f.getline(buffer, sizeof(buffer))) {
      check(p);
      std::string line = buffer;
      p.bytes += line.size() + 1;
      auto i = line.find_first_not_of(" \t\r");
      if (i != std::string::npos && line[i] == '[' &&
          line.find(']', i) != std::string::npos) {
        t = line.substr(i);
        return true;
      }
    }
    return false;
  }
  bool next(std::string &t) {
    t.clear();
    int c;
    if (first) {
      first = false;
      if (f.peek() == 0xef) {
        get();
        if (get() != 0xbb || get() != 0xbf)
          throw std::runtime_error("Invalid BOM");
      }
    }
    while ((c = get()) != EOF) {
      check(p);
      if (std::isspace((unsigned char)c))
        continue;
      if (c == ';') {
        while ((c = get()) != EOF && c != '\n') {
        }
        continue;
      }
      if (c == '{') {
        anchor = f.tellg();
        while ((c = get()) != EOF && c != '}') {
          if (c == '{')
            throw std::runtime_error("Nested or unterminated comment");
        }
        if (c == EOF)
          throw std::runtime_error(
              "Unterminated comment; remaining file cannot be recovered");
        continue;
      }
      if (c == '[') {
        anchor = f.tellg();
        t = "[";
        bool quoted = false, esc = false;
        while ((c = get()) != EOF) {
          t += (char)c;
          if (t.size() > 65536)
            throw std::runtime_error("Tag too long");
          if (c == '"' && !esc)
            quoted = !quoted;
          if (c == ']' && !quoted)
            return true;
          esc = c == '\\' && !esc;
        }
        throw std::runtime_error("Unterminated tag");
      }
      if (c == '(' || c == ')') {
        t = (char)c;
        return true;
      }
      anchor = f.tellg();
      t = (char)c;
      while ((c = f.peek()) != EOF && !std::isspace((unsigned char)c) &&
             c != '{' && c != ';' && c != '[' && c != '(' && c != ')' &&
             c != '$') {
        t += (char)get();
        if (t.size() > 1024)
          throw std::runtime_error(
              "PGN token too long; remaining file cannot be recovered");
      }
      return true;
    }
    return false;
  }
};
std::pair<std::string, std::string> tag(const std::string &t) {
  size_t i = 1;
  while (i < t.size() && std::isspace((unsigned char)t[i]))
    ++i;
  size_t a = i;
  while (i < t.size() && !std::isspace((unsigned char)t[i]) && t[i] != '"')
    ++i;
  auto n = t.substr(a, i - a);
  while (i < t.size() && std::isspace((unsigned char)t[i]))
    ++i;
  if (n.empty() || i >= t.size() || t[i++] != '"')
    throw std::runtime_error("Invalid tag");
  std::string v;
  bool ended = false;
  for (; i < t.size(); ++i) {
    if (t[i] == '"') {
      ended = true;
      ++i;
      break;
    }
    if (t[i] == '\\' && i + 1 < t.size())
      ++i;
    v += t[i];
  }
  if (!ended)
    throw std::runtime_error("Invalid tag value");
  while (i < t.size() && std::isspace((unsigned char)t[i]))
    ++i;
  if (i >= t.size() || t[i] != ']')
    throw std::runtime_error("Invalid tag ending");
  return {n, v};
}
bool result(const std::string &s) {
  return s == "1-0" || s == "0-1" || s == "1/2-1/2" || s == "*";
}
void validateFen(const std::string &s) {
  std::istringstream f(s);
  std::string board, turn, castle, ep;
  unsigned half, full;
  if (!(f >> board >> turn >> castle >> ep >> half >> full) || !full ||
      full > 30000 || half > 30000)
    throw std::runtime_error("Invalid FEN");
  int rank = 0, file = 0, wk = 0, bk = 0, w = 0, b = 0;
  for (char c : board) {
    if (c == '/') {
      if (file != 8)
        throw std::runtime_error("Invalid FEN rank");
      ++rank;
      file = 0;
    } else if (c >= '1' && c <= '8')
      file += c - '0';
    else {
      if (std::string("pnbrqkPNBRQK").find(c) == std::string::npos)
        throw std::runtime_error("Invalid FEN piece");
      ++file;
      wk += c == 'K';
      bk += c == 'k';
      (std::isupper((unsigned char)c) ? w : b)++;
    }
  }
  if (rank != 7 || file != 8 || wk != 1 || bk != 1 || w > 16 || b > 16 ||
      (turn != "w" && turn != "b") ||
      castle.find_first_not_of("KQkq-") != std::string::npos ||
      (ep != "-" && (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' ||
                     (ep[1] != '3' && ep[1] != '6'))))
    throw std::runtime_error("Invalid FEN / not standard chess");
}
std::vector<Event> process(const Game &g, const Config &cfg, Progress &p,
                           std::string *canonical = nullptr) {
  if (!g.error.empty())
    throw std::runtime_error(g.error);
  if (g.moves.empty())
    throw std::runtime_error("Game has no moves");
  if (g.tags.count("Variant") && g.tags.at("Variant") != "Standard" &&
      g.tags.at("Variant") != "Normal" && g.tags.at("Variant") != "Chess")
    throw std::runtime_error("Non-standard variant");
  board_t b;
  board_start(&b);
  if (g.tags.count("FEN")) {
    validateFen(g.tags.at("FEN"));
    board_from_fen(&b, g.tags.at("FEN").c_str());
    if (!board_is_ok(&b) || !board_can_play(&b))
      throw std::runtime_error("Invalid FEN position");
  }
  if (canonical) {
    char fen[256];
    if (!board_to_fen(&b, fen, sizeof(fen)))
      throw std::runtime_error("Failed to normalize starting position");
    std::istringstream f(fen);
    std::string field;
    canonical->clear();
    // Piece placement, turn, castling and en passant determine the start.
    // Clocks, headers, annotations and results are not part of game identity.
    for (unsigned i = 0; i < 4; ++i) {
      if (!(f >> field))
        throw std::runtime_error("Failed to normalize starting position");
      *canonical += field + " ";
    }
    canonical->push_back('\0');
  }
  auto r = g.tags.find("Result");
  std::string rs = r == g.tags.end() ? "*" : r->second;
  if (!result(rs))
    throw std::runtime_error("Invalid result");
  std::vector<Event> out;
  out.reserve(std::min<size_t>(g.moves.size(), cfg.ply));
  uint32_t actualPly = 0;
  for (size_t i = 0; i < g.moves.size(); ++i) {
    check(p);
    auto s = g.moves[i];
    while (!s.empty() && (s.back() == '!' || s.back() == '?'))
      s.pop_back();
    if (s == "e.p." || s == "ep")
      continue;
    if (s.rfind("0-0", 0) == 0)
      std::replace(s.begin(), s.end(), '0', 'O');
    if (s.empty() || s.size() > 64)
      throw std::runtime_error("Invalid SAN");
    int m = move_from_san(s.c_str(), &b);
    if (!m || !move_is_legal(m, &b))
      throw std::runtime_error("Illegal SAN: " + s);
    if (actualPly < cfg.ply) {
      unsigned score = 1;
      if (rs == "1-0")
        score = b.turn == White ? 2 : 0;
      else if (rs == "0-1")
        score = b.turn == Black ? 2 : 0;
      out.push_back({b.key, g.id, actualPly, (uint16_t)m, (uint8_t)score, 0});
    }
    if (canonical) {
      canonical->push_back(char(unsigned(m) >> 8));
      canonical->push_back(char(m));
    }
    move_do(&b, m);
    ++actualPly;
  }
  return out;
}
// Thread-owned counters: no contended atomic updates for diagnostics.
struct alignas(64) WorkMetrics {
  double processSeconds = 0, sortSeconds = 0, writeSeconds = 0,
         queueSeconds = 0;
  uint64_t files = 0, bytes = 0, mergeGroups = 0;
};
struct Measure {
  double &value;
  Clock::time_point start = Clock::now();
  ~Measure() {
    value += std::chrono::duration<double>(Clock::now() - start).count();
  }
};
using GameBatch = std::vector<Game>;
class Queue {
  std::deque<GameBatch> q;
  std::mutex m;
  std::condition_variable notEmpty, notFull;
  size_t max;
  bool done = false;

public:
  Queue(size_t n) : max(n) {}
  void close() {
    std::lock_guard<std::mutex> l(m);
    done = true;
    notEmpty.notify_all();
    notFull.notify_all();
  }
  void put(GameBatch g, Progress &p) {
    std::unique_lock<std::mutex> l(m);
    while (q.size() >= max && !p.stop)
      notFull.wait_for(l, std::chrono::milliseconds(100));
    check(p);
    q.push_back(std::move(g));
    l.unlock();
    notEmpty.notify_one();
  }
  bool take(GameBatch &g, Progress &p, WorkMetrics &metrics) {
    Measure timing{metrics.queueSeconds};
    std::unique_lock<std::mutex> l(m);
    while (q.empty() && !done && !p.stop)
      notEmpty.wait_for(l, std::chrono::milliseconds(100));
    if (p.stop || q.empty())
      return false;
    g = std::move(q.front());
    q.pop_front();
    l.unlock();
    notFull.notify_one();
    return true;
  }
};
// Comparator checks keep even large in-memory sorts cancellable.
// This overload uses the sequential sort policy, so exceptions propagate.
template <class T, class Less>
void cancellableSort(std::vector<T> &v, Less less, Progress &p) {
  check(p);
  size_t comparisons = 0;
  std::sort(v.begin(), v.end(), [&](const T &a, const T &b) {
    if ((++comparisons & 4095) == 0)
      check(p);
    return less(a, b);
  });
  check(p);
}
void writeRun(const fs::path &path, std::vector<Event> &v, Progress &p,
              WorkMetrics &metrics) {
  check(p);
  {
    Measure timing{metrics.sortSeconds};
    cancellableSort(v, lessEvent, p);
  }
  Measure timing{metrics.writeSeconds};
  check(p);
  std::ofstream f(path, std::ios::binary);
  constexpr size_t chunkEvents = 1048576 / sizeof(Event);
  for (size_t begin = 0; begin < v.size(); begin += chunkEvents) {
    check(p);
    const size_t count = std::min(chunkEvents, v.size() - begin);
    f.write(reinterpret_cast<const char *>(v.data() + begin),
            count * sizeof(Event));
    if (!f)
      throw std::runtime_error("Temporary disk full / write error");
  }
  check(p);
  f.close();
  if (!f)
    throw std::runtime_error("Temporary disk full / write error");
  ++metrics.files;
  metrics.bytes += v.size() * sizeof(Event);
  v.clear();
}
class Merge {
  struct Node {
    Event e;
    size_t i;
  };
  struct Cmp {
    bool operator()(const Node &a, const Node &b) const {
      return lessEvent(b.e, a.e);
    }
  };
  std::vector<std::ifstream> f;
  std::priority_queue<Node, std::vector<Node>, Cmp> heap;
  void advance(size_t i) {
    Event e;
    if (f[i].read((char *)&e, sizeof(e)))
      heap.push({e, i});
    else if (!f[i].eof() || f[i].gcount())
      throw std::runtime_error("Corrupt temporary file");
  }

public:
  Merge(const std::vector<fs::path> &paths) {
    f.reserve(paths.size());
    for (auto &path : paths) {
      f.emplace_back(path, std::ios::binary);
      if (!f.back())
        throw std::runtime_error("Temporary file missing");
    }
    for (size_t i = 0; i < f.size(); ++i)
      advance(i);
  }
  bool next(Event &e) {
    if (heap.empty())
      return false;
    auto n = heap.top();
    heap.pop();
    e = n.e;
    advance(n.i);
    return true;
  }
};
// Exact external sorting of complete normalized mainlines: no hash collisions,
// no unbounded in-memory set, and the earliest valid input copy always wins.
struct BookGame {
  uint64_t id = 0;
  std::string canonical;
  std::vector<Event> events;
  unsigned bucket = 0;
  size_t memory() const {
    return sizeof(BookGame) + canonical.capacity() +
           events.capacity() * sizeof(Event);
  }
};
// Hashing only routes identical games to one partition. Identity is still
// checked against the full canonical bytes, so hash collisions cannot remove
// games.
uint64_t partitionHash(const std::string &s) {
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : s) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}
bool lessGame(const BookGame &a, const BookGame &b) {
  if (a.canonical != b.canonical)
    return a.canonical < b.canonical;
  return a.id < b.id;
}
void putGame(std::ostream &f, const BookGame &g) {
  uint32_t n = g.canonical.size(), e = g.events.size();
  f.write((const char *)&g.id, sizeof(g.id));
  f.write((const char *)&n, sizeof(n));
  f.write((const char *)&e, sizeof(e));
  f.write(g.canonical.data(), n);
  f.write((const char *)g.events.data(), e * sizeof(Event));
  if (!f)
    throw std::runtime_error(
        "Deduplication: temporary disk full / write error");
}
bool getGame(std::istream &f, BookGame &g) {
  if (!f.read((char *)&g.id, sizeof(g.id))) {
    if (f.eof() && !f.gcount())
      return false;
    throw std::runtime_error("Corrupt temporary game file");
  }
  uint32_t n = 0, e = 0;
  f.read((char *)&n, sizeof(n));
  f.read((char *)&e, sizeof(e));
  if (!f || !n || n > 256 + 65536 * 2 || e > 10000)
    throw std::runtime_error("Corrupt temporary game file");
  g.canonical.resize(n);
  g.events.resize(e);
  f.read(g.canonical.data(), n);
  f.read((char *)g.events.data(), e * sizeof(Event));
  if (!f)
    throw std::runtime_error("Corrupt temporary game file");
  return true;
}
class GameMerge {
  std::vector<std::ifstream> files;
  std::vector<BookGame> slots;
  struct Compare {
    const std::vector<BookGame> *slots;
    bool operator()(size_t a, size_t b) const {
      return lessGame((*slots)[b], (*slots)[a]);
    }
  };
  std::priority_queue<size_t, std::vector<size_t>, Compare> heap;

public:
  GameMerge(const std::vector<fs::path> &paths)
      : slots(paths.size()), heap(Compare{&slots}) {
    files.reserve(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
      files.emplace_back(paths[i], std::ios::binary);
      if (!files.back())
        throw std::runtime_error("Temporary game file missing");
      if (getGame(files.back(), slots[i]))
        heap.push(i);
    }
  }
  bool next(BookGame &g) {
    if (heap.empty())
      return false;
    auto i = heap.top();
    heap.pop();
    g = std::move(slots[i]);
    if (getGame(files[i], slots[i]))
      heap.push(i);
    return true;
  }
};
void integer(std::ostream &f, uint64_t n, unsigned bytes) {
  for (int i = bytes - 1; i >= 0; --i)
    f.put(char(n >> (8 * i)));
}
uint64_t readInteger(std::istream &f, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; i++) {
    int c = f.get();
    if (c == EOF)
      throw std::runtime_error("Unexpected end of BIN");
    v = (v << 8) | (unsigned char)c;
  }
  return v;
}
struct Cleanup {
  fs::path path;
  ~Cleanup() {
    std::error_code e;
    fs::remove_all(path, e);
  }
};
fs::path createRunOutput(const fs::path &baseOutput) {
  if (baseOutput.filename().empty())
    throw std::runtime_error("Choose a BIN filename");
  auto parent = baseOutput.parent_path();
  if (parent.empty())
    parent = ".";
  fs::create_directories(parent);
  auto timestamp = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &timestamp);
#else
  localtime_r(&timestamp, &local);
#endif
  std::ostringstream name;
  name << "Run_" << std::put_time(&local, "%Y-%m-%d_%H-%M-%S");
  for (unsigned suffix = 0; suffix < 10000; ++suffix) {
    auto dir = parent /
               (name.str() + (suffix ? "_" + std::to_string(suffix + 1) : ""));
    std::error_code ec;
    if (fs::create_directory(dir, ec))
      return dir / baseOutput.filename();
    if (ec && ec != std::errc::file_exists)
      throw std::runtime_error("Cannot create run folder: " + ec.message());
  }
  throw std::runtime_error("Cannot allocate a unique run folder");
}
// Copy reports with checked streams, including zero-byte error reports.
// Avoid filesystem::copy_file overwrite behavior on Windows.
void copyReport(const fs::path &source, const fs::path &target) {
  std::ifstream in(source, std::ios::binary);
  std::ofstream out(target, std::ios::binary | std::ios::trunc);
  if (!in || !out)
    throw std::runtime_error("Cannot open report for copying");
  char buffer[16384];
  while (in.read(buffer, sizeof(buffer)) || in.gcount()) {
    out.write(buffer, in.gcount());
    if (!out)
      throw std::runtime_error("Report copy write error");
  }
  if (!in.eof())
    throw std::runtime_error("Report copy read error");
  out.close();
  if (!out)
    throw std::runtime_error("Report copy write error");
}
void build(const Config &cfg, Progress &p) {
  p.running = true;
  auto start = Clock::now();
  p.set("Reading PGN / processing games");
  fs::path work;
  Cleanup cleanup{fs::path{}};
  std::thread reader;
  std::vector<std::thread> workers;
  std::unique_ptr<Queue> queue;
  try {
    initialize();
    if (cfg.inputs.empty() || cfg.threads < 1 || cfg.threads > 256 ||
        cfg.ply < 1 || cfg.ply > 10000 || cfg.minGames < 1)
      throw std::runtime_error("Invalid settings");
    if (fs::exists(cfg.output) && !cfg.overwrite)
      throw std::runtime_error("Output already exists");
    auto dir = cfg.output.parent_path();
    if (dir.empty())
      dir = ".";
    if (!fs::is_directory(dir))
      throw std::runtime_error("Output folder does not exist");
    for (auto &i : cfg.inputs) {
      if (fs::equivalent(i, dir))
        throw std::runtime_error("Invalid input");
      p.totalBytes += fs::file_size(i);
    }
    work = dir / (".Pgn2BinMT-" +
                  std::to_string(Clock::now().time_since_epoch().count()));
    fs::create_directory(work);
    cleanup.path = work;
    size_t mem = cfg.memoryMiB ? cfg.memoryMiB : automaticMemoryMiB();
    size_t cap =
        std::max<size_t>(4096, mem * 1048576 / cfg.threads / sizeof(Event));
    queue = std::make_unique<Queue>(std::max<size_t>(2, cfg.threads));
    std::mutex rm, em;
    std::vector<WorkMetrics> workerMetrics(cfg.threads);
    std::vector<WorkMetrics> reducerMetrics(
        std::min<unsigned>(cfg.threads, 32));
    double readerSeconds = 0, readerQueueSeconds = 0;
    WorkMetrics finalMetrics;
    // Bound concurrent merge streams below the Windows CRT file limit.
    unsigned dedupThreads = std::min<unsigned>(cfg.threads, 32);
    size_t gameFanIn = std::min<size_t>(32, 384 / dedupThreads);
    std::vector<fs::path> runs;
    std::vector<std::vector<fs::path>> gameRuns(dedupThreads);
    std::vector<uint64_t> bucketCounts(dedupThreads, 0);
    std::string fatal;
    std::ofstream errors(work / "errors.txt");
    auto fail = [&](std::string e) {
      std::lock_guard<std::mutex> l(em);
      if (fatal.empty())
        fatal = std::move(e);
      p.stop = true;
      queue->close();
    };
    struct JoinGuard {
      Progress &p;
      Queue &q;
      std::thread &reader;
      std::vector<std::thread> &workers;
      ~JoinGuard() {
        bool pending = reader.joinable();
        for (auto &t : workers)
          pending |= t.joinable();
        if (pending) {
          p.stop = true;
          q.close();
          if (reader.joinable())
            reader.join();
          for (auto &t : workers)
            if (t.joinable())
              t.join();
        }
      }
    } guard{p, *queue, reader, workers};
    for (unsigned w = 0; w < cfg.threads; ++w)
      workers.emplace_back([&, w] {
        auto &metrics = workerMetrics[w];
        ++p.active;
        try {
          std::vector<Event> v;
          if (!cfg.deduplicate)
            v.reserve(cap);
          std::vector<BookGame> games;
          size_t gameMemory = 0;
          size_t gameBudget =
              std::max<size_t>(65536, mem * 1048576 / cfg.threads);
          unsigned batch = 0;
          auto flush = [&] {
            if (v.empty())
              return;
            auto path = work / ("w" + std::to_string(w) + "-" +
                                std::to_string(batch++) + ".run");
            writeRun(path, v, p, metrics);
            std::lock_guard<std::mutex> l(rm);
            runs.push_back(path);
          };
          auto flushGames = [&] {
            if (games.empty())
              return;
            check(p);
            {
              Measure timing{metrics.sortSeconds};
              cancellableSort(
                  games,
                  [](const auto &a, const auto &b) {
                    return a.bucket != b.bucket ? a.bucket < b.bucket
                                                : lessGame(a, b);
                  },
                  p);
            }
            Measure timing{metrics.writeSeconds};
            auto batchId = batch++;
            for (size_t begin = 0; begin < games.size();) {
              check(p);
              unsigned bucket = games[begin].bucket;
              size_t end = begin + 1;
              while (end < games.size() && games[end].bucket == bucket)
                ++end;
              auto path = work / ("games" + std::to_string(w) + "-" +
                                  std::to_string(batchId) + "-b" +
                                  std::to_string(bucket) + ".run");
              std::ofstream file(path, std::ios::binary);
              for (size_t i = begin; i < end; ++i) {
                check(p);
                putGame(file, games[i]);
              }
              file.close();
              if (!file)
                throw std::runtime_error("Deduplication: write error");
              ++metrics.files;
              metrics.bytes += fs::file_size(path);
              {
                std::lock_guard<std::mutex> lock(rm);
                gameRuns[bucket].push_back(path);
                bucketCounts[bucket] += end - begin;
              }
              begin = end;
            }
            games.clear();
            gameMemory = 0;
          };
          GameBatch batchGames;
          while (queue->take(batchGames, p, metrics)) {
            for (const Game &g : batchGames) {
              std::vector<Event> events;
              std::string canonical;
              try {
                Measure timing{metrics.processSeconds};
                events =
                    process(g, cfg, p, cfg.deduplicate ? &canonical : nullptr);
              } catch (const std::exception &e) {
                if (p.stop)
                  throw;
                ++p.invalid;
                std::lock_guard<std::mutex> l(em);
                errors << "Game " << g.id << ": " << e.what() << "\n";
                if (!errors)
                  throw std::runtime_error("Error report write failure");
                continue;
              }
              ++p.valid;
              p.events += events.size();
              if (cfg.deduplicate) {
                BookGame item{g.id, std::move(canonical), std::move(events)};
                item.bucket = partitionHash(item.canonical) % dedupThreads;
                if (!games.empty() && gameMemory + item.memory() > gameBudget)
                  flushGames();
                gameMemory += item.memory();
                games.push_back(std::move(item));
                if (gameMemory >= gameBudget)
                  flushGames();
                continue;
              }
              ++p.uniqueGames;
              for (auto &e : events) {
                v.push_back(e);
                if (v.size() >= cap)
                  flush();
              }
            }
          }
          if (!p.stop) {
            flush();
            flushGames();
          }
        } catch (const std::exception &e) {
          if (!p.stop)
            fail(e.what());
        }
        --p.active;
      });
    reader = std::thread([&] {
      Measure readerTiming{readerSeconds};
      try {
        uint64_t id = 0;
        GameBatch pending;
        size_t pendingBytes = 0;
        auto dispatch = [&] {
          if (pending.empty())
            return;
          {
            Measure timing{readerQueueSeconds};
            queue->put(std::move(pending), p);
          }
          pending = GameBatch{};
          pendingBytes = 0;
        };
        for (auto &path : cfg.inputs) {
          check(p);
          std::ifstream f(path, std::ios::binary);
          if (!f)
            throw std::runtime_error("Cannot open source file");
          Lexer lex(f, p);
          Game g;
          int depth = 0;
          size_t gameBytes = 0;
          bool oversized = false;
          auto emit = [&] {
            if (g.tags.empty() && g.moves.empty() && g.error.empty())
              return;
            if (depth)
              g.error = "Unterminated variation";
            g.id = ++id;
            ++p.read;
            // Bound batching by both game count and estimated payload. A single
            // large game travels alone, under the existing reader size limit.
            size_t bytes = sizeof(Game) + g.error.size() +
                           g.moves.size() * sizeof(std::string);
            for (const auto &move : g.moves)
              bytes += move.size();
            for (const auto &tag : g.tags)
              bytes += 96 + tag.first.size() + tag.second.size();
            if (!pending.empty() && pendingBytes + bytes > 65536)
              dispatch();
            pendingBytes += bytes;
            pending.push_back(std::move(g));
            if (pending.size() == 16 || pendingBytes >= 65536)
              dispatch();
            g = Game{};
            depth = 0;
            gameBytes = 0;
            oversized = false;
          };
          std::string t;
          auto nextToken = [&] {
            try {
              return lex.next(t);
            } catch (const std::exception &e) {
              if (p.stop)
                throw;
              g.error = std::string(e.what()) +
                        "; recover at next recognizable tag line, otherwise "
                        "remaining game boundaries unknown";
              emit();
              return lex.recover(t);
            }
          };
          try {
            while (nextToken()) {
              if (t[0] == '[') {
                std::pair<std::string, std::string> kv;
                try {
                  kv = tag(t);
                } catch (const std::exception &e) {
                  g.error = e.what();
                  continue;
                }
                if (!g.moves.empty() || g.tags.count(kv.first) || depth)
                  emit();
                gameBytes += t.size();
                if (gameBytes > 8 * 1048576 || g.tags.size() > 4096) {
                  oversized = true;
                  g.error = "Game headers too large";
                }
                if (!oversized)
                  g.tags[kv.first] = kv.second;
                continue;
              }
              if (t == "(") {
                ++depth;
                continue;
              }
              if (t == ")") {
                if (depth)
                  --depth;
                else
                  g.error = "Variation closing without opening";
                continue;
              }
              if (depth)
                continue;
              if (t[0] == '$')
                continue;
              if (result(t)) {
                if (g.tags.count("Result") && g.tags["Result"] != t)
                  g.error = "Header and movetext results differ";
                g.tags["Result"] = t;
                emit();
                continue;
              }
              size_t i = 0;
              while (i < t.size() && std::isdigit((unsigned char)t[i]))
                ++i;
              if (i < t.size() && t[i] == '.') {
                while (i < t.size() && t[i] == '.')
                  ++i;
                t = t.substr(i);
              }
              if (t.empty() || t == "...")
                continue;
              gameBytes += t.size();
              if (gameBytes > 8 * 1048576 || g.moves.size() >= 65536) {
                oversized = true;
                g.error = "Game exceeds 8 MiB; skipped";
                g.moves.clear();
              }
              if (!oversized)
                g.moves.push_back(t);
            }
            emit();
          } catch (const std::exception &e) {
            if (p.stop)
              throw;
            g.error = e.what();
            emit();
            throw std::runtime_error("PGN structure cannot be recovered in " +
                                     path.u8string() + ": " + e.what());
          }
        }
        dispatch();
        queue->close();
      } catch (const std::exception &e) {
        if (!p.stop)
          fail(e.what());
      }
    });
    reader.join();
    for (auto &t : workers)
      t.join();
    if (!fatal.empty())
      throw std::runtime_error(fatal);
    check(p);
    errors.close();
    if (!errors)
      throw std::runtime_error("Error report write failure");
    p.readMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                             Clock::now() - start)
                             .count();
    const auto processingEnd = Clock::now();
    if (cfg.deduplicate) {
      auto dedupStart = Clock::now();
      uint64_t workUnits = 0;
      unsigned nonempty = 0;
      for (unsigned bucket = 0; bucket < dedupThreads; ++bucket) {
        size_t count = gameRuns[bucket].size();
        unsigned passes = 1;
        while (count > gameFanIn) {
          count = (count + gameFanIn - 1) / gameFanIn;
          ++passes;
        }
        workUnits += bucketCounts[bucket] * passes;
        if (!gameRuns[bucket].empty())
          ++nonempty;
      }
      p.set("Deduplicating complete games (parallel)", workUnits);
      std::atomic<unsigned> nextBucket{0};
      std::vector<std::thread> reducers;
      struct ReducerJoin {
        Progress &progress;
        std::vector<std::thread> &threads;
        ~ReducerJoin() {
          bool any = false;
          for (auto &thread : threads)
            if (thread.joinable())
              any = true;
          if (any)
            progress.stop = true;
          for (auto &thread : threads)
            if (thread.joinable())
              thread.join();
        }
      } reducerGuard{p, reducers};
      auto reduce = [&] {
        ++p.dedupActive;
        try {
          for (;;) {
            check(p);
            unsigned bucket = nextBucket++;
            if (bucket >= dedupThreads)
              break;
            auto &metrics = reducerMetrics[bucket];
            auto &paths = gameRuns[bucket];
            if (paths.empty())
              continue;
            unsigned gamePass = 0;
            while (paths.size() > gameFanIn) {
              std::vector<fs::path> next;
              for (size_t i = 0; i < paths.size(); i += gameFanIn) {
                check(p);
                std::vector<fs::path> group(
                    paths.begin() + i,
                    paths.begin() + std::min(paths.size(), i + gameFanIn));
                auto dest = work / ("gmerge-b" + std::to_string(bucket) + "-p" +
                                    std::to_string(gamePass) + "-" +
                                    std::to_string(i) + ".run");
                {
                  ++metrics.mergeGroups;
                  Measure timing{metrics.writeSeconds};
                  GameMerge merge(group);
                  std::ofstream file(dest, std::ios::binary);
                  BookGame game;
                  while (merge.next(game)) {
                    check(p);
                    putGame(file, game);
                    ++p.phaseDone;
                  }
                  file.close();
                  if (!file)
                    throw std::runtime_error(
                        "Deduplication: merge write error");
                }
                ++metrics.files;
                metrics.bytes += fs::file_size(dest);
                for (const auto &path : group)
                  fs::remove(path);
                next.push_back(dest);
              }
              paths = std::move(next);
              ++gamePass;
            }
            {
              GameMerge merge(paths);
              BookGame game;
              std::string previous;
              bool havePrevious = false;
              std::vector<Event> events;
              size_t eventCap = std::max<size_t>(
                  4096, mem * 1048576 / dedupThreads / sizeof(Event));
              events.reserve(eventCap);
              unsigned batch = 0;
              auto flush = [&] {
                if (events.empty())
                  return;
                auto path = work / ("unique-b" + std::to_string(bucket) + "-" +
                                    std::to_string(batch++) + ".run");
                writeRun(path, events, p, metrics);
                std::lock_guard<std::mutex> lock(rm);
                runs.push_back(path);
              };
              while (merge.next(game)) {
                check(p);
                ++p.phaseDone;
                if (havePrevious && previous == game.canonical) {
                  ++p.duplicates;
                  continue;
                }
                previous = game.canonical;
                havePrevious = true;
                ++p.uniqueGames;
                for (const auto &event : game.events) {
                  events.push_back(event);
                  if (events.size() >= eventCap)
                    flush();
                }
              }
              flush();
            }
            for (const auto &path : paths)
              fs::remove(path);
            paths.clear();
          }
        } catch (const std::exception &error) {
          if (!p.stop)
            fail(error.what());
        }
        --p.dedupActive;
      };
      for (unsigned i = 0; i < std::min(dedupThreads, nonempty); ++i)
        reducers.emplace_back(reduce);
      for (auto &thread : reducers)
        thread.join();
      if (!fatal.empty())
        throw std::runtime_error(fatal);
      check(p);
      if (p.phaseDone != workUnits || p.uniqueGames + p.duplicates != p.valid)
        throw std::runtime_error("Deduplication accounting mismatch");
      p.dedupMilliseconds =
          std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                                dedupStart)
              .count();
    }
    const auto dedupEnd = Clock::now();
    uint64_t retainedEvents = 0;
    for (const auto &path : runs)
      retainedEvents += fs::file_size(path) / sizeof(Event);
    p.set("Merging intermediate results", retainedEvents);
    unsigned pass = 0;
    while (runs.size() > 32) {
      p.set("Merging intermediate results (pass " + std::to_string(pass + 1) +
                ")",
            retainedEvents);
      std::vector<fs::path> next;
      for (size_t i = 0; i < runs.size(); i += 32) {
        check(p);
        std::vector<fs::path> group(
            runs.begin() + i, runs.begin() + std::min(runs.size(), i + 32));
        auto dest = work / ("merge" + std::to_string(pass) + "-" +
                            std::to_string(i) + ".run");
        {
          ++finalMetrics.mergeGroups;
          Merge merge(group);
          std::ofstream f(dest, std::ios::binary);
          Event e;
          while (merge.next(e)) {
            check(p);
            f.write((char *)&e, sizeof(e));
            ++p.phaseDone;
          }
          f.close();
          if (!f)
            throw std::runtime_error("Merge: disk full");
        }
        ++finalMetrics.files;
        finalMetrics.bytes += fs::file_size(dest);
        for (auto &path : group)
          fs::remove(path);
        next.push_back(dest);
      }
      runs = std::move(next);
      ++pass;
    }
    const auto mergeEnd = Clock::now();
    p.set("Filtering / writing BIN", retainedEvents);
    auto temp = work / "Book.part";
    std::ofstream bin(temp, std::ios::binary);
    Merge merge(runs);
    Event e;
    uint64_t key = 0;
    bool have = false;
    struct Stat {
      uint32_t count = 0, sum = 0;
      uint64_t games = 0, last = 0;
    };
    std::map<uint16_t, Stat> stats;
    auto flushPosition = [&] {
      if (!have)
        return;
      ++p.positions;
      std::vector<std::pair<uint16_t, Stat>> selected;
      for (auto &s : stats)
        if (s.second.games >= cfg.minGames && s.second.sum)
          selected.push_back(s);
      std::sort(selected.begin(), selected.end(), [](auto &a, auto &b) {
        if (a.second.sum != b.second.sum)
          return a.second.sum > b.second.sum;
        return a.first < b.first;
      });
      for (auto &s : selected) {
        integer(bin, key, 8);
        integer(bin, s.first, 2);
        integer(bin, s.second.sum, 2);
        integer(bin, 0, 4);
        ++p.records;
      }
      stats.clear();
    };
    while (merge.next(e)) {
      check(p);
      ++p.phaseDone;
      if (!have || key != e.key) {
        flushPosition();
        key = e.key;
        have = true;
      }
      auto &s = stats[e.move];
      ++s.count;
      s.sum += e.score;
      if (s.last != e.id) {
        ++s.games;
        s.last = e.id;
      }
      if (s.count >= 16384)
        for (auto &v : stats) {
          v.second.count = (v.second.count + 1) / 2;
          v.second.sum = (v.second.sum + 1) / 2;
        }
    }
    flushPosition();
    bin.close();
    if (!bin)
      throw std::runtime_error("BIN write error / disk full");
    const auto binEnd = Clock::now();
    p.set("Validating BIN", p.records);
    {
      std::ifstream f(temp, std::ios::binary);
      uint64_t previous = 0, n = 0;
      while (f.peek() != EOF) {
        check(p);
        auto k = readInteger(f, 8);
        auto m = readInteger(f, 2), w = readInteger(f, 2),
             l = readInteger(f, 4);
        if ((n && k < previous) || !m || !w || l)
          throw std::runtime_error("BIN validation failed");
        previous = k;
        ++n;
        ++p.phaseDone;
      }
      if (n != p.records || fs::file_size(temp) != n * 16)
        throw std::runtime_error("BIN size mismatch");
    }
    const auto validationEnd = Clock::now();
    p.set("Writing reports / finalizing", 3);
    double seconds =
        std::chrono::duration<double>(Clock::now() - start).count();
    auto report = work / "report.txt";
    std::ofstream r(report);
    auto timestamp = std::time(nullptr);
    r << "Pgn2BinMT RC2.9\nDate/time: "
      << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %H:%M:%S") << "\n";
    for (auto &i : cfg.inputs)
      r << "Source: " << i.u8string() << "\n";
    r << "Read: " << p.read << "\nValid: " << p.valid
      << "\nRejected: " << p.invalid
      << "\nDeduplication: " << (cfg.deduplicate ? "on" : "off")
      << "\nDuplicate games skipped: " << p.duplicates
      << "\nContributing games: " << p.uniqueGames << "\nPly: " << cfg.ply
      << "\nMinimum distinct games per move: " << cfg.minGames
      << "\nWeight: PolyGlot standard (sum 2/1/0; *=1; halve at 16384 in "
         "source order)\nThreads: "
      << cfg.threads
      << "\nDeduplication seconds: " << p.dedupMilliseconds / 1000.0
      << "\nDeduplication thread limit: "
      << (cfg.deduplicate ? dedupThreads : 0) << "\nBuffer budget MiB: " << mem
      << "\nBuffer strategy: "
      << (cfg.deduplicate ? "bounded complete-game buffers; exact external sort"
                          : "bounded event buffers")
      << "\nUnique positions before filtering: " << p.positions
      << "\nBIN records: " << p.records << "\nBIN bytes: " << p.records * 16
      << "\nSeconds: " << seconds
      << "\nGames/second: " << p.read / std::max(.001, seconds)
      << "\nMainline only. Deduplication compares the starting position and "
         "complete "
         "mainline exactly; the first valid source copy contributes.\n";
    auto duration = [](auto a, auto b) {
      return std::chrono::duration<double>(b - a).count();
    };
    r << std::fixed << std::setprecision(6)
      << "\nDiagnostics: wall phases are sequential; worker sums overlap and "
         "must not be added to wall time.\n"
      << "Processing phase seconds: " << duration(start, processingEnd)
      << "\nDeduplication phase wall seconds: "
      << duration(processingEnd, dedupEnd)
      << "\nEvent merge phase seconds: " << duration(dedupEnd, mergeEnd)
      << "\nBIN writing phase seconds: " << duration(mergeEnd, binEnd)
      << "\nBIN validation phase seconds: " << duration(binEnd, validationEnd)
      << "\nReader seconds (overlaps workers): " << readerSeconds
      << "\nReader queue seconds (included in reader): " << readerQueueSeconds
      << "\nQueue batch limit games: 16\nQueue batch target bytes: 65536\n";
    auto summarize = [&](const char *label, const auto &all) {
      WorkMetrics total;
      for (const auto &m : all) {
        total.processSeconds += m.processSeconds;
        total.sortSeconds += m.sortSeconds;
        total.writeSeconds += m.writeSeconds;
        total.queueSeconds += m.queueSeconds;
        total.files += m.files;
        total.bytes += m.bytes;
        total.mergeGroups += m.mergeGroups;
      }
      r << label << " process seconds sum: " << total.processSeconds << "\n"
        << label << " sort seconds sum: " << total.sortSeconds << "\n"
        << label << " temporary write/merge seconds sum: " << total.writeSeconds
        << "\n"
        << label << " queue wait/handoff seconds sum: " << total.queueSeconds
        << "\n"
        << label << " temporary files written: " << total.files << "\n"
        << label << " temporary bytes written: " << total.bytes << "\n"
        << label << " intermediate merge groups: " << total.mergeGroups << "\n";
    };
    summarize("PGN workers", workerMetrics);
    summarize("Dedup reducers", reducerMetrics);
    r << "Event intermediate merge groups: " << finalMetrics.mergeGroups
      << "\nEvent intermediate files written: " << finalMetrics.files
      << "\nEvent intermediate bytes written: " << finalMetrics.bytes
      << "\nEvent intermediate merge passes: " << pass
      << "\nTiming scope: Seconds excludes report copying, final rename and "
         "cleanup.\n";
    r.close();
    if (!r)
      throw std::runtime_error("Report write error");
    copyReport(work / "errors.txt", dir / "Pgn2BinMT_Errors.txt");
    ++p.phaseDone;
    copyReport(report, dir / "Pgn2BinMT_Report.txt");
    ++p.phaseDone;
    check(p);
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), cfg.output.c_str(),
                     MOVEFILE_WRITE_THROUGH |
                         (cfg.overwrite ? MOVEFILE_REPLACE_EXISTING : 0)))
      throw std::runtime_error("Final BIN rename failed");
#else
    if (!cfg.overwrite && fs::exists(cfg.output))
      throw std::runtime_error("Output was created in the meantime");
    fs::rename(temp, cfg.output);
#endif
    p.set("Process ended: OK", 1);
    p.phaseDone = 1;
  } catch (const std::exception &e) {
    p.stop = true;
    if (queue)
      queue->close();
    if (reader.joinable())
      reader.join();
    for (auto &t : workers)
      if (t.joinable())
        t.join();
    std::error_code ec;
    if (!work.empty()) {
      auto dir = cfg.output.parent_path();
      if (dir.empty())
        dir = ".";
      if (fs::exists(work / "errors.txt")) {
        try {
          copyReport(work / "errors.txt",
                     dir / "Pgn2BinMT_Errors_Incomplete.txt");
        } catch (...) {
          // Preserve the primary failure even if its supplementary report
          // fails.
        }
      }
      std::ofstream failed(dir / "Pgn2BinMT_Incomplete.txt");
      failed << "Run incomplete: " << e.what() << "\nRead: " << p.read
             << "\nValid processed: " << p.valid
             << "\nRejected processed: " << p.invalid
             << "\nNot yet processed: " << (p.read - p.valid - p.invalid)
             << "\nThreads: " << cfg.threads
             << "\nElapsed seconds before cleanup: "
             << std::chrono::duration<double>(Clock::now() - start).count()
             << "\nNo new final BIN was placed.\n";
    }
    {
      std::lock_guard<std::mutex> l(p.mutex);
      p.error = e.what();
      p.phase = p.error == "Stopped" ? "Process ended: Stopped"
                                     : "Process ended: Failed";
    }
  }
  p.running = false;
}
} // namespace mt

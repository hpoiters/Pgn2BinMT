// Exercises cancellation inside the actual sort used by disk-run writers.
#include "../src/core.cpp"
#include <iostream>
#include <random>
int main() {
  mt::Progress progress;
  std::vector<mt::Event> events(200000);
  std::mt19937_64 random(123);
  for (auto &event : events) {
    event.key = random();
    event.id = random();
    event.ply = random() % 60;
  }
  auto expected = events;
  std::sort(expected.begin(), expected.end(), mt::lessEvent);
  mt::cancellableSort(events, mt::lessEvent, progress);
  for (size_t i = 0; i < events.size(); ++i)
    if (mt::lessEvent(events[i], expected[i]) ||
        mt::lessEvent(expected[i], events[i]))
      return 1;
  std::shuffle(events.begin(), events.end(), random);
  size_t calls = 0;
  try {
    mt::cancellableSort(
        events,
        [&](const auto &a, const auto &b) {
          if (++calls == 5000)
            progress.stop = true;
          return mt::lessEvent(a, b);
        },
        progress);
    return 2;
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()) != "Stopped" || calls > 8192)
      return 3;
  }
  std::cout << "Sort order unchanged; cancellation detected within 4096 "
               "comparisons\n";
}

Historical development record. See RELEASE_VALIDATION.md for current status.

# Pgn2BinMT RC2.9: independent review and bounded queue revision

3 October 2026. Scope: RC2.8 source and supplied Kingbase run evidence.

**Update: user Windows/Kingbase RC2.9 processing runs at 2 and 18 threads are now verified; see the final follow-up section.**

## Outcome

The original queue performs two `notify_all()` operations per game, using one
condition variable for producer and consumers. Every available game can wake
many workers competing for the same mutex. A targeted-only experiment reduces
elapsed time and voluntary context switches on a short-game workload. Batching
handoffs reduces those costs further. This establishes an avoidable queue cost
on the tested Linux system, not the complete cause of the user's Windows results.

RC2.9 implements separate producer/consumer conditions, `notify_one()` after
unlocking during normal handoff, and batches of at most 16 games with an estimated
64 KiB payload target. A larger game is sent alone, under the existing reader
limits. Queue capacity remains max(2, selected workers), now counted in batches;
there can also be a batch in each worker and in the reader. This increases
bounded input buffering compared with RC2.8. The payload target is an estimate,
not a process-memory cap. Sorting buffers retain the existing shared budget.
EOF flushes the final partial batch. Close/failure wakes both sets of waiters;
external cancellation is still checked during waits and inside game processing.

No changes were made to chess rules, canonical identity, scoring, filtering,
source IDs, deduplication partitions, merge ordering, or the default thread count.
GUI changes are limited to the version label. File dialogs, dated output folders,
completion popups, phase progress, Stop behavior and icon remain in the source.

## Historical Windows evidence

The supplied completed runs process 2,157,700 games; 2,154,515 valid including
104,347 duplicates; 3,185 rejected; 2,050,168 contributing. The 24,395,664-byte
BIN and rejection logs are identical across all completed old-version runs.
Fastest measured total is 274.645 s at 2 threads; 18 on J: takes 858.937 s and
36 takes 1060.840 s. The source PGN was not supplied. This review does not
independently validate those source-level deduplication/rejection decisions.

The original PC has 18 physical cores / 36 logical processors. This Linux
environment has a CPU quota of 8 CPUs and reports 9 available CPUs via nproc.
18/36-thread local tests therefore oversubscribe this environment. No numerical
local speedup is a prediction for the Windows PC or Kingbase workload.

## Experimental method

Baseline CLI compiled from untouched RC2.8 core, same native compiler and -O3
flags as the new CLI. The final RC2.9 comparison includes diagnostics overhead.
Builds run serially, with reversed build/thread order on alternating repetitions.
Caches are not flushed; background load and warm/cold state are not controlled.
Results are medians, not confidence intervals. Short and long sources use three
repetitions per build/count; the low-memory unique stress uses two. All final
comparisons use minimum 1, 60 ply, deduplication enabled, and nonempty BIN output.
Every resulting BIN is compared byte for byte across all builds and counts.

Fixtures:
- 200,000 identical six-ply games: deliberately stresses handoff overhead.
- 36,000 longer games, 120 distinct legal randomly generated lines repeated
  300 times, up to 75 ply: includes legal move processing beyond book depth.
- 6,000 generated legal games up to 60 ply, overwhelmingly unique, at 1 MiB:
  deliberately forces temporary-file fragmentation and multipass merging.

The first two workloads are duplicate-heavy and do not represent Kingbase's
duplicate ratio. The third is small and intentionally uses much less memory
than the user's 4096 MiB. Raw results and reports are in tests/rc29-*-benchmark.json.

## Measured medians (seconds)

| Workload | Threads | RC2.8 | RC2.9 | Old/new ratio |
|---|---:|---:|---:|---:|
| short | 2 | 1.708 | 0.879 | 1.94 |
| short | 6 | 2.815 | 0.903 | 3.12 |
| short | 18 | 4.504 | 0.959 | 4.70 |
| short | 36 | 5.476 | 0.950 | 5.77 |
| long | 2 | 1.426 | 1.307 | 1.09 |
| long | 6 | 1.603 | 0.734 | 2.18 |
| long | 18 | 1.961 | 0.973 | 2.01 |
| long | 36 | 2.030 | 1.217 | 1.67 |
| unique | 2 | 0.285 | 0.278 | 1.03 |
| unique | 6 | 0.310 | 0.266 | 1.17 |
| unique | 18 | 0.717 | 0.686 | 1.05 |
| unique | 36 | 0.967 | 0.953 | 1.01 |

At 18 threads the short-game median voluntary context switches fall from 1,000,791 to 22,174. These are OS scheduler counters for the whole child, not a direct count of queue wake-ups. The targeted-only experiment (no batching) also improved time and context switches; raw exploratory data is in tests/rc29-wakeup-isolation.json. Its minimum-3 duplicate fixture produces an empty BIN by design; semantic validation relies on the final nonempty comparisons and independent regression tests.

## Remaining bottleneck and proposed next change

In the low-memory unique stress, RC2.9 creates 40 initial game files at 2 threads,
360 at 6, 2562 at 18 and 3627 at 36 in the first repetition. At 36, roughly
0.715 s of the 0.953 s median-scale operation is spent in the processing phase
in that first run. The final comparison shows little benefit at 36 for this
workload. Queue changes do not resolve the original worker/bucket file fan-out.

Each worker flush sorts by bucket then canonical game and creates one file for
each occupied bucket. More workers reduce each worker's buffer, and the number
of buckets also rises (capped at 32). Thus both handoff and storage organization
can be problematic. Instrumentation records actual files, bytes and merge work.
It does not prove that file creation accounts for all processing time, nor that
the same mechanism dominates the 4096 MiB Windows run.

The next justified redesign, if Windows reports show temporary-write/merge costs
dominating, is to store sorted bucket segments in one file per worker flush with
a range index, then give reducers bounded range readers. This avoids one file
per worker/bucket/flush while retaining exact canonical comparisons and source
IDs. It requires new range-boundary, handle-budget, cancellation and output
identity tests. It is deliberately not bundled into this queue experiment.
Changing partition count independently of reducer count is another measurable
option. Neither simply reducing the default nor claiming all scaling is fixed
is warranted by the evidence.

## Diagnostics and limitations

The report has nonoverlapping wall phases for processing, deduplication,
event merges, BIN writing and validation. Reader duration overlaps workers.
Per-worker process, sort, temporary write/merge and queue totals are elapsed
sums over workers; they overlap and are not CPU utilization or disk-only times.
Queue timing includes lock acquisition, wait, handoff and old-batch disposal.
Temporary write/merge sums include some bookkeeping and, for intermediate
dedup merges, reading. Counts include rewritten runs. Intermediate merge groups
are counted, and event merge passes are explicit; dedup passes are represented
by merge groups, not a separate pass total. Cleanup/report copying/final rename
remain outside reported Seconds. Detailed lock profiling and cleanup timing
were not added. Cancelled runs now record selected threads and elapsed time.

Metrics are thread-owned and summed after join, avoiding new shared atomic
updates for the measurements. Per-game clock reads still have overhead. The
final build, including that overhead, is what the comparison measures.

## Validation and Windows acceptance

Passed: 27 independent core checks; 17 dedup checks; 30 batch-boundary runs
(1/15/16/17/33 games, ordinary/40 KB tags, 1/2/36 threads); existing safety and
dated-folder checks; cancellation during sort and deduplication; phase-progress
and actual parallel-reducer monitoring (peak 18). Existing output is preserved
on cancellation and temporary files removed. Tests cover halving/source-order,
FEN, fragments, legal moves, external merge paths, final partial batches and
phase-time accounting. No new claim of exhaustive parser correctness is made.

The Windows x64 GUI cross-build succeeds using the existing MinGW POSIX toolchain.
PE subsystem is Windows GUI, icon resources remain embedded and imports require
only Windows DLLs. The Windows EXE has not been executed in this environment;
UI acceptance and real Kingbase correctness/performance still require the user PC.

When testing resumes, first compare RC2.9 at 2 and 18 threads using the same
Kingbase source on J: and 4096 MiB. Preserve the dated folders. Check the report
counts and byte identity against the old BIN, then compare phase times. No need
to repeat the entire thread sweep before that comparison.

## Reproduction

From the package root:

```sh
./build-linux.sh
g++ -std=c++17 -O3 -DNDEBUG -pthread -Isrc src/cli.cpp \
  tests/reference/core-rc28.cpp src/polyglot/*.cpp -o build/rc28-cli
python3 tests/make_queue_benchmarks.py /tmp/pgn-queue-inputs
python3 tests/benchmark_queue.py build/rc28-cli build/Pgn2BinMT-cli \
  /tmp/pgn-queue-inputs/short.pgn /tmp/short-results.json
python3 tests/benchmark_queue.py build/rc28-cli build/Pgn2BinMT-cli \
  /tmp/pgn-queue-inputs/benchmark.pgn /tmp/long-results.json
python3 tests/benchmark_queue.py build/rc28-cli build/Pgn2BinMT-cli \
  /tmp/pgn-queue-inputs/unique.pgn /tmp/unique-results.json --memory 1 --repeats 2
```

Requires python-chess/chess 1.11.2 for fixture generation. Historical baseline
core source and a patch are included for reproducibility. No external repository
was modified or published.

## Windows follow-up, 3 October 2026, 22:03 local

Two user RC2.9 Kingbase runs completed on J:, with the same reported 60 ply,
minimum 3, dedup on, 4096 MiB. Source folder changed to the RC2.9 directory.
2 threads: 262.091 s versus 274.645 s previously (4.57% shorter).
18 threads: 252.803 s versus 858.937 s previously (70.57% shorter, 3.40x speed).
RC2.9 at 18 is 9.288 s faster than at 2. These are individual user runs with
uncontrolled background/cache conditions, not a universal thread optimum.

Both BINs are byte-identical to the old Kingbase reference and all counts match.
Error logs differ in order only: all 3185 game-ID/message lines and their
multiplicities match exactly. Raw reports are in tests/rc29-kingbase-user-results.json.

Processing phase: 240.251/236.643 s at 2/18 threads; dedup: 11.274/3.267 s;
BIN writing: 10.388/12.714 s; validation: 0.177/0.178 s. Reader duration overlaps
workers: 226.503/230.707 s, with only 0.211/0.208 s in queue handoff/wait.
This suggests serial reading/tokenization is the next limiting stage; it does
not identify disk I/O as the cause. Worker wait sums overlap across threads.
Initial temporary file counts are 4/324, with identical 3,376,426,932 bytes
written. No intermediate dedup merge groups or event merge passes occurred.
These 4096 MiB runs therefore do not support additional merge passes as the
current dominant cost. Profile the reader before prioritizing file redesign.

This supersedes the earlier pending Windows/Kingbase processing validation for
these two runs. It does not retest every UI function or independently validate
the unavailable source PGN. No executable change accompanied this update.

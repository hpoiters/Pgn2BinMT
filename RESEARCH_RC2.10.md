Historical development record. See RELEASE_VALIDATION.md for current status.

# Pgn2BinMT RC2.10: buffered reader

Date: 2026-10-03. This is a release candidate, not a Windows performance claim.

## Change and reason
RC2.9 Kingbase reports showed a reader duration around 230 seconds and only
0.21 seconds in reader queue handoff/wait. This suggests a serial input/parser
limit. RC2.10 replaces per-character istream get/peek and per-token tellg with
a 256 KiB input buffer and a logical byte offset. The old stream was already
buffered: this is not a claim that every character previously read the disk.
The remaining tellg is only on the error-recovery path. Recovery seeks to the
same logical anchor and uses the same bounded line scan as RC2.9. Read-ahead is
discarded on recovery. Byte offsets are valid because input files open in binary
mode. Stop is also checked every 64 KiB consumed, including within comments.

This isolates the reader optimization. Game tokenization remains serial;
parallel tokenization and game-boundary splitting are future candidates.
Queue batches, source-order IDs, legal-move checks, full-game deduplication,
weighting, minimum-game filtering and BIN layout remain unchanged. All GUI
behavior is retained; only the displayed release number changes. The reader
adds one 256 KiB buffer, outside the existing approximate work-buffer budget.

## Verification
- 27 core cases with independent python-chess expected records passed.
- 17 whole-game deduplication checks passed.
- 30 queue batch boundary runs passed.
- 36 old/new comparisons passed: block boundaries, BOM, CRLF, long comments,
  variations, malformed tags/tokens/comments, EOF and recovery. BIN bytes,
  counts and the multiset of error lines agree.
- Existing-output protection, controlled Stop/cleanup and invalid CLI argument
  checks passed. Three dated run folders preserved earlier outputs.
- Both benchmark fixtures produced identical nonempty BIN bytes for every
  release/thread count/repetition.
- Windows x64 GUI cross-build succeeded; imports are Windows system DLLs.
  The Windows executable and native GUI have not been run in this Linux host.

## Measurements
Median of three runs per release/thread count, alternating execution order;
128 MiB work budget, minimum games 1, deduplication on. Short fixture: 200,000
repeated six-ply games. Long fixture: 36,000 games, 120 distinct games repeated
300 times, up to 75 ply. Linux host CPU quota: eight CPUs; 18/36 worker tests
are oversubscribed. Runs are short and subject to noise. No cache flushing.

| Fixture | Threads | RC2.9 seconds | RC2.10 seconds | Time reduction |
|---|---:|---:|---:|---:|
| short | 2 | 0.829 | 0.848 | -2.2% |
| short | 6 | 0.831 | 0.546 | 34.3% |
| short | 18 | 0.973 | 0.671 | 31.0% |
| short | 36 | 0.984 | 0.722 | 26.7% |
| long | 2 | 1.348 | 1.336 | 0.9% |
| long | 6 | 0.735 | 0.491 | 33.2% |
| long | 18 | 0.973 | 0.753 | 22.6% |
| long | 36 | 1.213 | 1.078 | 11.1% |

The short two-thread case regressed slightly in this sample; more threads still
do not guarantee a speedup. These fixtures are not Kingbase and the measured
percentages must not be projected onto Windows. This experiment changes block
reading and position tracking together; it does not isolate tellg's individual
cost. Original Kingbase PGN is unavailable here.

## Reproduction
Build current CLI with build-linux.sh. Build the old comparator by substituting
 tests/reference/core-rc29.cpp for src/core.cpp in that command (keep -Isrc).
Run tests/test_buffered_reader.py with that comparator's absolute path.
Use tests/make_queue_benchmarks.py to recreate fixtures as documented in the
RC2.9 research report; tests/benchmark_reader.py compares both builds.
Raw results: tests/rc210-short-benchmark.json, rc210-long-benchmark.json and
rc210-reader-results.json. tests/rc210-core.patch records the core change.
Historical result files retain their original version labels; they are not
claims of new RC2.10 execution.

## Next Windows check
Use the same merged Kingbase source and settings (60 ply, minimum 3, dedup on,
4096 MiB). Compare 2 and 18 threads first with RC2.9's 262.091 and 252.803 seconds.
Repeat timings before choosing a default. Compare BIN SHA256 to
b3a4eecc6a7c099bcbe23897ce4bbdaa2a4bf09c48b3829f3598d4a7d26f8b8c,
all game counts, and error lines including multiplicity (order may differ).
Keep phase and reader timings to see whether the bottleneck has moved.

## Design references
These sources informed the design, but no source code was imported:
- Intel oneTBB, Controlling Chunking: task size versus scheduling overhead.
  https://www.intel.com/content/www/us/en/docs/onetbb/developer-guide-api-reference/2021-12/controlling-chunking.html
- pgn-reader: fixed-buffer streaming PGN parsing and separated move validation.
  https://github.com/niklasf/rust-pgn-reader (now merged into shakmaty)
- simdjson parse_many: buffer reuse and overlapping parsing stages.
  https://github.com/simdjson/simdjson/blob/master/doc/parse_many.md

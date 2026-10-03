Historical development record. See RELEASE_VALIDATION.md for current status.

# Pgn2BinMT RC2 verification — 3 October 2026

The supplied Windows x86-64 executable was rebuilt from the included source.
The common processing core passed 30 existing correctness checks, 17 new
deduplication checks, three existing file-safety checks and an additional
cancellation check during deduplication. These core tests ran natively on Linux.

The new Windows GUI could not be run in this build environment. The English
labels, checkbox placement, progress-reset behavior and Windows dialogs still
need practical confirmation on Windows. No claim is made that the new GUI has
been visually tested. RC1 was previously exercised on the user's Windows PC;
that does not validate the changes in RC2.

## New deduplication checks

- Earliest valid source copy wins, including its result, with identical BIN
  bytes for 1, 2, 6 and 18 threads.
- Differences in headers, comments, variations, NAGs and results are ignored.
- Duplicate copies do not satisfy the minimum-games filter.
- Disabling deduplication retains the previous weights and minimum counts.
- Duplicates across separate files are removed together.
- An invalid earlier game does not suppress a later valid copy.
- Different continuations beyond book depth remain distinct.
- A shorter fragment remains distinct from a longer game.
- Explicit standard-start FEN equals implicit standard start; move counters
  are ignored when determining game identity.
- Alternate castling notation normalizes to the same move sequence.
- Matching SAN with different starting boards remains distinct.
- The revised default demo contains three genuinely different full games.
- A 50,000-game source (25,000 unique) forces bounded disk batches and external
  merge passes at a 1 MiB budget. All games share the first move while differing
  beyond book depth, forcing original-order weight halving. Outputs with
  1 and 18 threads match the independent python-chess-derived expectation
  and are byte-identical. Exactly 25,000 duplicate games are skipped.
- A 300,000-game source at 18 threads and a 1 MiB budget retains three unique
  games, skips 299,997 duplicates and produces the expected ten records.

Deduplication uses exact normalized game data rather than a hash. Temporary
complete-game records are externally sorted; memory use does not depend on
retaining all game identities in one in-memory set.

Results: tests/dedup-results.json. Recorded times include the independent
Python comparison and are test durations, not Windows performance benchmarks.

## Existing core and file-safety checks

The 30 regression checks retain explicit --no-dedup to compare classic PolyGlot
behavior. They cover independent key/move/weight comparisons, classic PolyGlot
2.0.4, FEN/special moves, depth, invalid input, recovery, minimum games,
transpositions, repetitions and original-order weight halving.
Results: tests/results.json.

Existing output is protected without overwrite authorization. Controlled
cancellation preserves that output, cleans temporary files and writes an
incomplete report. A missing CLI option value is handled without a crash.
Results: tests/safety-results.json.

The additional monitor stops the core specifically during the new
"Deduplicating complete games" phase. The existing BIN remains byte-identical;
temporary files are removed and an incomplete report is written.
Source: tests/test_stop_dedup.cpp. Results: tests/dedup-stop-results.json.

Earlier RC1 measurements remain in TESTVERSLAG.md, tests/results-small.json,
tests/results-million.json and tests/benchmark-results.json. These are historical
RC1 results, not new RC2 runs or an estimate of deduplication performance.

## Windows acceptance check

Extract into a separate folder and run Pgn2BinMT.exe. Select tests/basic.pgn,
keep the default 60 ply / minimum 3 / Remove duplicate games checked:
3 valid, 0 rejected, 0 duplicates, 10 BIN records, 160 bytes.
Compare with tests/Book.bin. After completion, change a source checkbox: the
previous progress and counters should disappear. Verify the English controls,
window resizing, adding/clearing sources, output choice and controlled stop.

When comparing worker speeds, use the same source and conversion options,
including the same deduplication setting. Neither this suite nor historical
RC1 timings establish that 18 threads are optimal on the user's i9-10980XE.

## RC2.1 status refinement

Successful completion now displays Ready! rather than Done. Terminal speed and
counters remain frozen. Failure reasons appear in the status; View report
selects the incomplete report after failure or cancellation. Windows and native
executables were rebuilt, and a demo success/failure check passed. The GUI
changes require practical Windows verification. The cause of the user's RC1
Kingbase failure remains unknown pending its incomplete-run report.

## RC2.2 wording

Terminal status now uses Process ended: OK, Process ended: Failed or
Process ended: Stopped. Counters stay frozen in all three cases. Rebuilt
executables and checked successful and failed completion natively.

RC2.2 also adds an explicit completion popup for success and manual stop, and
a failure popup with the error reason. Popup behavior requires Windows testing.

## RC2.3 run folders and report copying

Each GUI run gets a unique dated subfolder under the same chosen OUTPUT folder.
The chosen BIN basename is retained. Open output folder, View report and the
success popup use the actual run path, while the output field remains the base
choice. Same-second collisions receive a numeric suffix. Native checks confirm
separate runs preserve previous BINs and reports and retain an existing base BIN.

The user's RC1 report revealed filesystem::copy_file failing with File exists
while copying a report after all 2,157,700 input games were processed. Report
copying now uses checked streams, including empty error reports, and avoids
that filesystem overwrite call. Native checks verify overwrite of both empty
and nonempty error reports. Windows runtime confirmation remains necessary.
See tests/run-folder-results.json.

## RC2.4 progress per phase

Added phase-specific work counters. Deduplication and each merge pass reset
the progress bar. Known totals use games, events or BIN records as appropriate;
unknown-length phases use a marquee. Read rate freezes after PGN processing,
and PGN workers then shows finished. Completion wording and popups are retained.
Native deduplication tests were rerun to verify unchanged output and cancellation.
Windows progress-bar behavior still requires practical testing.

All 17 deduplication checks passed again. The new phase-monitor test observed
partial deduplication progress, checked work counters against their totals and
confirmed final 100% progress. Cancellation during deduplication, run-folder
isolation and the three safety checks also passed again. See
tests/phase-progress-results.json and tests/dedup-stop-results.json.

## RC2.5 parallel deduplication

Whole-game candidates are partitioned during PGN worker spills. Independent
reducers merge and deduplicate partitions in parallel, then produce sorted
event runs with original IDs preserved. Routing hashes are not identity checks;
full canonical comparison still decides duplicates and earliest-ID selection.
The thread limit is min(selected threads, 32); merge fan-in bounds combined
input streams to 384. Temporary buffers remain shared by budget rather than
allocating the full budget to each worker. An exception/stop joins all reducers
before temporary cleanup. Dedup workers is shown separately from PGN workers.
The deduplication phase progress includes all required merge-pass game records.

The 17 deduplication tests, safety, run-folder preservation, progress and stop
checks are rerun for this build. Windows GUI execution remains untested here.

RC2.5 results: all 17 deduplication checks passed. A monitored run reached
18 simultaneous reducers, stayed within phase-counter totals and produced a
BIN byte-identical to RC2.4. The updated stop-during-deduplication and phase
monitor tests passed, as did run-folder preservation and the three safety tests.
See tests/parallel-progress-results.json and the other result JSON files.

An exploratory 288,000-game native benchmark (120 unique complete games repeated,
512 MiB buffers) produced identical BINs with 1, 6 and 18 threads:

| Threads | Deduplication seconds | Whole run seconds |
|---:|---:|---:|
| 1 | 0.180 | 27.4218 |
| 6 | 0.049 | 13.8669 |
| 18 | 0.189 | 16.1755 |

Six threads reduced deduplication time in this test, while 18 did not. These
short deduplication timings on a highly repetitive synthetic source do not
establish the optimal thread count on the user's 18-core Windows PC. This is
verification of parallel operation, with an observed speed benefit at six
threads, not a promise of universal acceleration. Small repeated measurements
are in tests/dedup-benchmark-small-results.json; this larger exploratory run
is in tests/dedup-benchmark-results.json.

## RC2.6 — cancellation inside large sorts and event writes

Added cancellation checks every 4096 comparisons in the sequential event and
whole-game sort paths. Event output is split into at most 1 MiB chunks with
checks between writes. Sort exceptions propagate through existing RAII
cleanup and worker error handling. No parallel execution policy is used.
A deterministic test requests cancellation during the real sort comparator
and checks both detection latency in comparisons and unchanged normal sort
order. Safety, dated output folders and complete-game deduplication regressions
are rerun. Windows GUI behavior still requires user acceptance testing.

## RC2.7 — executable-relative default output

GUI startup ignores historical LastOutput settings and starts with
programDir/Output/Book.bin. Defaults restores the same path. Custom output
selections remain effective for subsequent runs until Defaults or restart.
LastSource and conversion settings remain persisted. No core processing
logic was changed. Existing run-folder checks verify creation of timestamped
subfolders and preservation of earlier results. Windows GUI path behavior
requires acceptance testing.

User acceptance evidence for RC2.6 (2026-10-03): screenshot shows
Process ended: OK popup and timestamped output folder after the Kingbase run.
Read 2,157,700; valid 2,154,515; rejected 3,185; duplicates 104,347;
BIN records 1,524,729. Approximately 14.5 minutes from start to screenshot;
exact elapsed and deduplication times have not been supplied from the report.

## RC2.8 — application artwork

Added original SVG artwork and a multiresolution ICO containing
16, 24, 32, 48, 64, 128 and 256 pixel images. Both Windows build scripts
compile app.rc with windres and link the resource object. The main window
class loads icon resource 1. Visual asset inspection covers small and large
sizes. The supplied EXE resource tree is checked for embedded icon entries.
Conversion logic is unchanged from RC2.7; Windows shell/window icon rendering
requires user acceptance testing.

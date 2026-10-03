# Pgn2BinMT 2.0 release validation

2026-10-04. Application behavior follows RC2.11, with one timing correction:
on exception/Stop, after joining reader/workers, record the reading duration
if it has not already been recorded. Preserve it if a later phase is stopped.
This is done before the terminal phase is published. No parsing, chess,
deduplication, weighting or BIN-writing algorithm is changed.

## User Windows validation of RC2.11

Completed run Run_2026-10-04_00-00-02: 18 workers, 60 ply, minimum 3,
deduplication on, 4096 MiB. Total 55.1441 seconds; deduplication 2.602751 seconds.
BIN byte-identical to the previous Kingbase reference; 1,524,729 records,
24,395,664 bytes. SHA256:
b3a4eecc6a7c099bcbe23897ce4bbdaa2a4bf09c48b3829f3598d4a7d26f8b8c.
Read 2,157,700 = valid 2,154,515 + rejected 3,185.
Valid = contributing 2,050,168 + duplicates 104,347.
All error lines including IDs and multiplicity match the reference.

Stop run Run_2026-10-04_00-08-19: stopped, no new BIN placed.
Read 1,078,386 = valid processed 1,076,730 + rejected 1,555 + pending 101.
Elapsed before cleanup 17.918 seconds (not the Stop-button response latency).
The screenshot confirms the Stop popup. A further completed run's screenshot
confirms the success popup and its dated output path.

## Final-build checks

Targeted stop-rate regression covers stopping while reading, a nonzero frozen
duration, and preserving that duration when stopped during deduplication.
Safety and dated-output-folder checks verify controlled cleanup, protection of
previous output, separate run folders and the expected 160-byte sample BIN.
The final Windows x64 GUI is cross-compiled from the included sources.
The corrected Windows executable has not been run in this Linux build host;
previous user GUI checks apply to RC2.11. No claim of a new full Kingbase run
on 2.0 is made. Original Kingbase source was not available for independent
re-evaluation of rejected games. Source comparisons and package hashes are
included in the build manifest and SHA256SUMS.txt.

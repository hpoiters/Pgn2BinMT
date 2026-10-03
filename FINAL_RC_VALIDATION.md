# Pgn2BinMT RC2.11 — final release candidate

2026-10-03. The processing implementation is frozen at tested RC2.10.
Every application source file was compared with RC2.10: only the report and
window version strings differ. No conversion or GUI behavior change is intended.
The Windows x64 GUI and Linux CLI were rebuilt. Fresh CLI checks passed for
three dated output folders, previous output preservation, controlled Stop,
temporary cleanup, malformed CLI arguments and default 160-byte sample output.
The new Windows GUI has not been executed in the build environment.

## Windows evidence from RC2.10

The user supplied two completed Kingbase runs:

| Workers | Total seconds | Processing | Deduplication | BIN writing |
|---|---:|---:|---:|---:|
| 2 | 125.701 | 103.666 | 11.416 | 10.441 |
| 18 | 53.5767 | 38.042 | 2.343 | 13.003 |

Same reported depth 60 ply, minimum 3, deduplication on, 4096 MiB. These are
individual observations; hardware/cache/background load were not independently
verified. RC2.9 took 262.091 and 252.803 seconds with 2 and 18 workers.

Both RC2.10 BIN files are byte-identical to the previous reference:
SHA256 b3a4eecc6a7c099bcbe23897ce4bbdaa2a4bf09c48b3829f3598d4a7d26f8b8c.
Both contain 1,524,729 records / 24,395,664 bytes.
Read 2,157,700 = valid 2,154,515 + rejected 3,185.
Valid 2,154,515 = contributing 2,050,168 + duplicates 104,347.
All 3,185 error lines, including game IDs and multiplicity, match the earlier
reference. Their order is not required to match. Original PGN was unavailable
for an independent re-evaluation of rejected games.
Raw fields are in tests/rc210-kingbase-user-results.json.

## Final user check

1. Extract this package into a new folder and start Pgn2BinMT.exe. Confirm RC2.11.
2. Select the same Kingbase file; use 18 workers, 60 ply, minimum 3,
   deduplication enabled, 4096 MiB.
3. Let it finish. Confirm Process ended: OK, the completion popup and frozen
   terminal counters. Open output folder and View report must target this run.
4. Start a separate trial and press Stop during processing. Wait for
   Process ended: Stopped and its popup; retain the incomplete-run report.
5. Send both run folders for comparison. Earlier successful output must remain.

This is still a release candidate pending that final Windows check.

## Package contents and language

EXE, complete build sources, assets, scripts, English instructions, attribution,
research and test evidence are included. Superseded Dutch narrative documents
are retained in earlier release packages rather than distributed in this one.
Historical research/results retain their original version scope. This file is
the current validation summary; earlier pending-test statements describe history.
BuildManifest.json identifies current sources and executable. SHA256SUMS.txt
covers the shipped files. Neither is a code-signing certificate.

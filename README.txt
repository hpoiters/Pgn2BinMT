Pgn2BinMT 2.0 — Windows 10/11, 64-bit

Quick start
1. Extract the complete ZIP into a separate folder.
2. Run Pgn2BinMT.exe. No installation, extra runtime DLLs or .NET required.
3. Click Add PGN or Browse folder, then check the files you want to process.
4. Choose threads, Depth (ply), Minimum games per move and the output filename.
5. Leave Remove duplicate games checked to count duplicate games only once.
6. Click Build BIN. Open output folder and View report show the results.

Choose file selects the base OUTPUT folder and BIN filename. Each run creates
its own dated subfolder INSIDE that same folder, for example
output/Run_2026-10-03_13-20-00/Book.bin. The chosen BIN name is preserved.
Runs started in the same second get a numeric suffix. Existing runs and reports
are retained; you do not need to rename the BIN for each run.
Open output folder opens the most recent run's subfolder. The completion popup
shows the actual BIN path. The output field continues to show the base choice.

Default settings: half the logical processors (18 workers on an i9-10980XE),
60 ply = 30 full moves, minimum 3 distinct games per move, automatic RAM,
standard PolyGlot weighting and whole-game deduplication enabled.
Conversion settings are saved for the next launch. Defaults resets conversion
options and restores Output/Book.bin beside this EXE.

Thread count and performance
The default is half the logical processors. More threads do not guarantee
proportional speed: input parsing, synchronization, sorting and storage can
limit throughput. One PGN supplies all workers; manual splitting is not required.
RC2.9 reduced queue synchronization costs. RC2.10 added block-buffered input.
Version 2.0 retains that processing code and fixes the frozen rate after Stop.

User-reported Kingbase timings, seconds (individual runs, not controlled trials):
Threads       RC2.9       RC2.10
2             262.091    125.701
18            252.803     53.5767
Settings: 60 ply, minimum 3, deduplication on, 4096 MiB. Reported source: same
merged Kingbase PGN. Hardware/background/cache conditions were not independently
verified. Both RC2.10 BINs match the previous reference byte for byte. All game
counts and the error-line multisets match; error ordering can differ.
These are RC2.10 measurements, not timings of this RC2.11 executable.

Compare identical input/settings and run one conversion at a time. The report's
Seconds includes processing through BIN validation, excluding report copying,
final rename and cleanup. Reader and worker timing sums overlap; do not add
them to calculate elapsed time. A full phase bar does not mean the run is done.

New report diagnostics
Processing, deduplication, event merge, BIN writing and validation phase times
are sequential elapsed times. Reader and worker/reducer totals overlap: never
add their sums to obtain elapsed time. Queue time includes lock/wait/handoff.
Process sums cover legal move processing and normalization. Temporary write
sums can include merge reading and bookkeeping; they are not pure disk time.
Counters record temporary files and bytes written (including rewritten merge
runs), intermediate merge groups and event merge passes. This helps separate
queue costs from sorting and storage work. Report copying/cleanup are outside
these timings. Incomplete reports now include threads and elapsed seconds.

What counts as a duplicate?
The normalized starting position and the COMPLETE legal mainline must match,
including moves beyond the selected book depth. Headers, players, dates,
comments, NAGs, variations, result and FEN move counters do not determine game
identity. Different continuations or a different starting position remain
separate games. A shorter fragment is distinct from a longer game.

The FIRST valid copy in source-file/list order contributes its result and
weights. Later copies are skipped, even if their results differ. Deduplication
covers all checked files together and happens after reading and validating
all games. Invalid copies never suppress a valid game. Uncheck Remove duplicate games to reproduce RC1 weighting and minimum-count behavior.

This is a move-based definition: separately played games with identical
starting position and entire mainline also count once when deduplication is on.

Progress
The progress bar restarts for each processing phase. During PGN reading it
measures input bytes; during deduplication it measures game records handled across the required
merge passes and duplicate comparisons; during event merging and BIN writing it measures retained events handled;
during validation it measures BIN records checked. Additional event merge passes
restart the bar and show their pass number. Unknown-length phases show an
animated bar. A full bar describes only the displayed phase, not the whole run.
Only Process ended: OK and the completion popup confirm successful completion.
PGN workers shows finished after input processing; later phases still run.
Read rate freezes when PGN processing finishes and is not the speed of later
phases. All terminal counters freeze when the process ends.
Duplicate counts appear during the deduplication phase. Valid includes all
valid input copies; Valid = contributing games + skipped duplicates. Read =
Valid + Rejected after successful completion. Collected records includes raw
records from valid copies before deduplication; BIN records is the final count.

Changing source checkboxes, adding a new source or clearing the source list
resets the previous progress bar and status. After successful completion, Status shows Process ended: OK and the completion speed
stays fixed. Failed and Stopped also freeze the counters. On failure the status
includes the error reason and View report opens the incomplete-run report. Source changes are disabled during a run.
Stop requests controlled shutdown and leaves any existing BIN intact.
Closing the window during a run requests a controlled stop first.

Sources and chess rules
Only the mainline contributes. The entire mainline is validated, even past the
book depth. An invalid game contributes no partial opening. Standard chess and
valid FEN starting positions are supported; Chess960 and variants are rejected.
A ply is one half-move: 61 ply = 30.5 full moves. Depth starts at each game's
starting position. Empty games are rejected.

Newly added files are unchecked. Browse folder recursively searches that folder
and its subfolders. Program folder searches the executable's folder, never a
parent folder. Output/results and internal temporary folders are skipped;
Add PGN can select an individual file in a skipped folder. Clear empties the
source list. The source folder is remembered. Each launch starts with Output/Book.bin
beside this EXE. Choose file changes the output folder/name for this session.

Weights and BIN format
Win/draw/loss weights are 2/1/0 for the side to move. Unknown results (*) count
as 1, as in classic PolyGlot. Zero-weight moves are omitted. Classic halving at
16384 is applied in original source order, independently of thread scheduling.
The minimum filter counts distinct contributing games per move: repetitions
within one game do not increase that minimum. Weighting still counts all
occurrences within a retained game. The minimum filter intentionally uses
actual distinct games rather than classic PolyGlot's halved internal counters.

Standard 16-byte big-endian PolyGlot records: key 8 bytes, move 2, weight 2,
learn 4 (zero). Keys are sorted; moves have deterministic ordering.

Reports (English)
Pgn2BinMT_Report.txt: sources, options, valid/rejected/skipped/contributing
counts, timings and output size.
Pgn2BinMT_Errors.txt: rejected games and reasons, with original input game IDs.
Pgn2BinMT_Incomplete.txt and Pgn2BinMT_Errors_Incomplete.txt: incomplete runs.
Each GUI run keeps these reports beside its BIN in its dated subfolder.
Earlier run folders remain intact, making comparisons easier.
The development CLI uses the exact output path unless --run-folder is specified.

Memory and disk
Windows automatically chooses a buffer budget of one quarter of available
physical RAM, bounded to 128–4096 MiB. Bounded game, queue and file buffers add
some overhead. Deduplication uses exact external sorting without relying on hash identity
or an unbounded in-memory set. Each merge uses at most 32 input streams;
parallel deduplication uses at most 384 input streams combined.

Temporary files are in a unique .Pgn2BinMT-... subfolder of the output folder.
Each candidate game stores its full normalized move sequence (2 bytes per
half-move plus starting position), metadata and up to Depth records of 24 bytes
each. Deduplication and sorting need additional passes and disk space.
For 21 million games at 60 ply, raw event data alone can approach 30 GB.
Full-game data adds to that, and merging can temporarily require roughly twice
the intermediate data size. Leave ample free disk space. Temporary files are
removed on success, controlled stop and handled errors. After a process crash,
remove only old .Pgn2BinMT-... folders when the program is closed.

Malformed-input limits
SAN/FEN errors are logged per game. Broken PGN structure is recovered at the
next recognizable header where possible. Completely damaged boundaries cannot
always yield an exact remaining game count; the error report says so.
Games exceeding 8 MiB text / 65536 move tokens, oversized headers or tokens
above 1024 bytes are rejected.

Verification
The complete RC2.11 Kingbase run took 55.1441 seconds with 18 workers and
produced a BIN byte-identical to the existing reference. User screenshots
confirmed both successful-completion and controlled-Stop popups.
Version 2.0 corrects only the missing terminal read timer on Stop/failure;
conversion rules are unchanged. Targeted native timer and safety checks cover
the correction. The rebuilt Windows GUI has not been run in the build host.
With default settings, tests/basic.pgn produces 3 valid unique games, no
rejections/duplicates and 10 BIN records (160 bytes).
See RELEASE_VALIDATION.md for evidence and limitations.

Source and license
All application source is in src. Derived PolyGlot chess code is credited in
THIRD_PARTY.md. GPL-2.0-or-later; see COPYING. This tool builds BIN files; CTG/BIN
switching and engine book configuration belong to the chess GUI/engine.

Completion popup
A popup confirms Process ended: OK, Failed or Stopped. Successful completion
shows the output filename; failure shows the reason. After clicking OK, the
terminal status and frozen speed remain in the main window. Closing the
application during processing stops it and closes without a completion popup.

Parallel deduplication (RC2.5)
The selected thread count also drives parallel deduplication, up to 32 threads.
For an 18-thread selection, up to 18 Dedup workers can work at once. Few distinct
games or an uneven distribution may leave fewer workers active. Identical
normalized games are routed to the same group. Full exact comparisons, not
hash identity, determine duplicates; the earliest valid source copy still wins.
Results are independent of scheduling and thread count.

Each reducer uses a bounded share of the buffer budget. Fan-in is adjusted so
parallel merges together use at most 384 input streams, below the Windows CRT
file limit with room for output streams. Disk speed and bandwidth can limit
scaling; 18 threads do not guarantee an 18-fold speedup. The report records
deduplication time and the configured deduplication thread limit.

Responsive Stop (RC2.6)
Large event and game sorts check cancellation every 4096 comparisons.
Event runs are written in chunks of at most 1 MiB, with cancellation checks
between chunks. Stop displays Stopping... please wait and disables repeated
clicks. Process ended: Stopped and its popup follow cleanup. An operating
system disk operation or temporary-file cleanup can still delay completion.

Output location (RC2.7)
Each launch defaults to Output beside the executable, independent of the
Windows working directory or a previous release's saved output folder.
Build BIN creates that folder when needed and adds a unique Run_timestamp
subfolder. Choose file or editing Output overrides the destination for
subsequent runs in the same session. Defaults restores Output/Book.bin
beside the EXE. Open output folder and View report still target the last run.

Application icon (RC2.8)
An original chess knight, conversion arrow and book icon is embedded in the
EXE and used by the main window. Multiple resolutions from 16 to 256 pixels
are included. The artwork sources are in assets; no external icon file is
needed when running the EXE. Processing behavior is unchanged from RC2.7.

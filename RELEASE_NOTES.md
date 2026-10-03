# Pgn2BinMT 2.0

Native Windows 10/11 x64 PGN-to-PolyGlot BIN converter.

- Multithreaded processing with buffered input and batched work distribution.
- Exact whole-game deduplication across selected files.
- Configurable book depth and minimum contributing games per move.
- Standard PolyGlot weighting and deterministic output.
- Phase progress, controlled Stop and completion popups.
- Each run receives a dated subfolder under Output beside the EXE by default.
- Defaults to half the logical processors; no source is automatically selected.
- English interface, reports and documentation; complete build source included.

The final correction freezes the read-rate timer after Stop or failure during
PGN processing. Previously the displayed speed could jump to an unrealistic
value after cancellation. Book contents and conversion rules are unchanged.

The preceding RC2.11 processed 2,157,700 Kingbase games in 55.14 seconds with
18 workers in a user run. Its BIN exactly matched the earlier reference.
Performance varies with hardware, input and settings. This is not a measured
timing of the final 2.0 executable. See RELEASE_VALIDATION.md.

Extract the ZIP and run Pgn2BinMT.exe. No installation or .NET required.
The executable is unsigned. License: GPL-2.0-or-later; see COPYING and
THIRD_PARTY.md.

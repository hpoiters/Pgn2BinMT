# Pgn2BinMT 2.0

Multithreaded PGN to PolyGlot BIN converter for Windows 10/11 x64, with exact whole-game deduplication.

## Download and run

Download the Windows ZIP from [Releases](https://github.com/hpoiters/Pgn2BinMT/releases), extract it into a separate folder, and run `Pgn2BinMT.exe`. No installation or .NET is required. The executable is unsigned.

## Features

- Buffered PGN input and batched parallel processing.
- Exact whole-game deduplication across selected files.
- Configurable book depth and minimum contributing games per move.
- Standard PolyGlot weighting and deterministic output.
- Progress for each phase, controlled Stop, and completion popups.
- A dated subfolder for every run under `Output` beside the EXE by default.
- Defaults to half the logical processors.
- English interface, reports, and documentation.

## Documentation

- [User guide](README.txt)
- [Build instructions](BUILD.md)
- [Release notes](RELEASE_NOTES.md)
- [Validation and limitations](RELEASE_VALIDATION.md)
- [Third-party credits](THIRD_PARTY.md)

More threads do not guarantee proportional speed. See the user guide for measured results and an explanation of the processing phases.

## License

GPL-2.0-or-later. See [COPYING](COPYING) and [THIRD_PARTY.md](THIRD_PARTY.md).

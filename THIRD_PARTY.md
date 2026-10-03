# Third-party attribution

Board, SAN, move legality, move handling and PolyGlot hashing derive from
Fabien Letouzey's PolyGlot, obtained from https://github.com/sshivaji/polyglot
at commit f46ee068860d363ace27004ec4da588bf4b48147.

Required source files are included in src/polyglot. See COPYING for GPL terms.
Original authors and contributors retain their rights. Pgn2BinMT is a derived
program distributed under GPL-2.0-or-later.

Local changes to that code:
- util.cpp: my_fatal throws a local exception instead of terminating the process,
  allowing a damaged game to be rejected individually.
- san.cpp: move_from_san checks san_to_lan's return value and returns MoveNone
  on failure.

The PGN tokenizer, multithreaded pipeline, external sorting, deterministic
aggregation, minimum filter, BIN writer, reports and Windows GUI are application
code in src/core.cpp, core.h, windows.cpp and cli.cpp.

Independent test dependency (not needed by or linked into the Windows EXE):
python-chess 1.11.2, https://github.com/niklasf/python-chess, GPL-3.0-or-later.
It is not bundled as a library in this package.

Optional classic test reference: Ubuntu polyglot 2.0.4+git20210322-1,
program version 2.0.4, GPL. This external reference is not linked into the EXE.

"""Compare RC2.9 and the buffered reader at input block/recovery boundaries.

Usage: python test_buffered_reader.py /path/to/rc29-cli
"""
import collections
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OLD = Path(sys.argv[1]).resolve()
NEW = ROOT / 'build/Pgn2BinMT-cli'
GAME = b'[Event "Boundary"]\n[Result "1-0"]\n\n1. e4 e5 2. Nf3 Nc6 1-0\n\n'
cases = {
    'plain': GAME * 3,
    'bom-crlf': b'\xef\xbb\xbf' + (GAME * 3).replace(b'\n', b'\r\n'),
    'comment': b'{' + b'x' * 800000 + b'}\n' + GAME,
    'line-comment': b';' + b'x' * 800000 + b'\n' + GAME,
    'recovery-nested': GAME + b'{broken {comment\n' + GAME * 2,
    'recovery-long-tag': GAME + b'[Event "' + b'x' * 70000 + b'\n' + GAME * 2,
    'recovery-long-token': GAME + b'x' * 1100 + b'\n' + GAME * 2,
    'recovery-eof': GAME + b'{bad {\n[Event "last"]',
    'no-newline': GAME.rstrip(),
    'empty': b'',
    'invalid-bom': b'\xef\xbbX' + GAME,
    'variations': b'[Event "V"]\n1. e4 (1. d4 {a} d5) e5 *\n' + GAME,
}
fields = ['Read', 'Valid', 'Rejected', 'Duplicate games skipped',
          'Contributing games', 'BIN records', 'BIN bytes']
results = []
with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp)
    for name, data in cases.items():
        for shift in (0, 262143, 262140):
            src = base / 'input.pgn'
            src.write_bytes(b' ' * shift + data)
            observations = []
            for label, exe in [('old', OLD), ('new', NEW)]:
                dest = base / label
                dest.mkdir(exist_ok=True)
                for file in dest.iterdir():
                    file.unlink()
                proc = subprocess.run([str(exe), '--threads', '6', '--min-games', '1',
                    '--output', str(dest / 'Book.bin'), str(src)],
                    capture_output=True, timeout=30)
                report = dest / 'Pgn2BinMT_Report.txt'
                values = dict(line.split(': ', 1) for line in report.read_text().splitlines()
                              if ': ' in line) if report.exists() else {}
                errors = dest / 'Pgn2BinMT_Errors.txt'
                out = dest / 'Book.bin'
                observations.append((proc.returncode, out.read_bytes() if out.exists() else None,
                    {key: values.get(key) for key in fields},
                    collections.Counter(errors.read_bytes().splitlines()) if errors.exists() else {}))
            assert observations[0] == observations[1], (name, shift)
            results.append({'case': name, 'prefix_bytes': shift, 'passed': True})
print(json.dumps(results, indent=2))
(ROOT / 'tests/rc210-reader-results.json').write_text(json.dumps(results, indent=2) + '\n')

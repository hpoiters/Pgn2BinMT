"""Compare two CLI builds serially on one source; never run with other tests.

Usage: python benchmark_queue.py OLD NEW SOURCE RESULT_JSON [--memory 128]
Linux context-switch counters are optional evidence, not a Windows prediction.
"""
import argparse
import hashlib
import json
import platform
import subprocess
import tempfile
import time
from pathlib import Path

try:
    import resource
except ImportError:
    resource = None

ap = argparse.ArgumentParser()
ap.add_argument('old', type=Path)
ap.add_argument('new', type=Path)
ap.add_argument('source', type=Path)
ap.add_argument('results', type=Path)
ap.add_argument('--memory', type=int, default=128)
ap.add_argument('--repeats', type=int, default=3)
args = ap.parse_args()
rows = []
reference = None
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp) / 'Book.bin'
    for repeat in range(args.repeats):
        counts = [2, 6, 18, 36]
        if repeat % 2:
            counts.reverse()
        for threads in counts:
            versions = [('RC2.8', args.old), ('RC2.9', args.new)]
            if repeat % 2:
                versions.reverse()
            for label, exe in versions:
                before = resource.getrusage(resource.RUSAGE_CHILDREN) if resource else None
                start = time.perf_counter()
                result = subprocess.run([
                    str(exe.resolve()), '--threads', str(threads),
                    '--memory-mib', str(args.memory), '--min-games', '1',
                    '--overwrite', '--output', str(out), str(args.source.resolve())
                ], capture_output=True, text=True, timeout=600)
                elapsed = time.perf_counter() - start
                assert result.returncode == 0, result.stderr
                data = out.read_bytes()
                assert data, 'Benchmark must exercise nonempty BIN output'
                if reference is None:
                    reference = data
                assert data == reference, 'Output differs across builds or thread counts'
                report = (Path(tmp) / 'Pgn2BinMT_Report.txt').read_text()
                fields = dict(line.split(': ', 1) for line in report.splitlines() if ': ' in line)
                row = dict(repeat=repeat+1, version=label, threads=threads,
                           elapsed_seconds=elapsed, sha256=hashlib.sha256(data).hexdigest(),
                           report=fields)
                if resource:
                    after = resource.getrusage(resource.RUSAGE_CHILDREN)
                    row['voluntary_context_switches'] = after.ru_nvcsw - before.ru_nvcsw
                    row['cpu_seconds'] = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
                rows.append(row)
                args.results.write_text(json.dumps(dict(
                    platform=platform.platform(), source_sha256=hashlib.sha256(args.source.read_bytes()).hexdigest(),
                    memory_mib=args.memory, cache_policy='No cache flushing; alternating build order; warm/cold not controlled',
                    rows=rows), indent=2)+'\n')
                print(label, threads, round(elapsed, 3), flush=True)

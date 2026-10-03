import hashlib,json,subprocess,tempfile,time,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('source',type=Path);ap.add_argument('--repeats',type=int,default=3);args=ap.parse_args();results=[];hashes=set()
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 for threads in (1,6,18):
  for repeat in range(args.repeats):
   out=d/f'book-{threads}-{repeat}.bin';start=time.perf_counter()
   run=subprocess.run([str(ROOT/'build/Pgn2BinMT-cli'),'--threads',str(threads),'--memory-mib','512','--min-games','1','--output',str(out),str(args.source)],capture_output=True,text=True,check=True,timeout=120)
   elapsed=time.perf_counter()-start
   report=(d/'Pgn2BinMT_Report.txt').read_text();fields=dict(line.split(': ',1) for line in report.splitlines() if ': ' in line)
   digest=hashlib.sha256(out.read_bytes()).hexdigest();hashes.add(digest)
   results.append({'threads':threads,'repeat':repeat+1,'total_seconds':round(elapsed,4),'dedup_seconds':float(fields['Deduplication seconds']),'valid':int(fields['Valid']),'duplicates':int(fields['Duplicate games skipped']),'records':int(fields['BIN records']),'sha256':digest})
 assert len(hashes)==1
(ROOT/'tests/dedup-benchmark-results.json').write_text(json.dumps(results,indent=2)+'\n')
for threads in (1,6,18):
 import statistics
 runs=[r for r in results if r['threads']==threads]
 print(threads,'threads: median dedup',statistics.median(r['dedup_seconds'] for r in runs),'s; total',statistics.median(r['total_seconds'] for r in runs),'s')

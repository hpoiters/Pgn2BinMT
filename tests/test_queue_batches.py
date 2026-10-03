"""Verify count and payload batch boundaries, including the final partial batch."""
import hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/Pgn2BinMT-cli'
results=[]
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 for count in [1,15,16,17,33]:
  for large in [False,True]:
   games=[]
   for i in range(count):
    name='x'*40000 if large else 'batch'
    games.append(f'[Event "{name}"]\n[Result "1-0"]\n\n1. e4 e5 2. Nf3 Nc6 1-0\n\n')
   src=d/'input.pgn';src.write_text(''.join(games));hashes=[]
   for threads in [1,2,36]:
    out=d/'Book.bin'
    r=subprocess.run([str(BIN),'--threads',str(threads),'--memory-mib','1','--min-games','1','--overwrite','--output',str(out),str(src)],capture_output=True,text=True,timeout=20)
    assert r.returncode==0,r.stderr
    report=(d/'Pgn2BinMT_Report.txt').read_text()
    f=dict(line.split(': ',1) for line in report.splitlines() if ': ' in line)
    assert int(f['Read'])==int(f['Valid'])==count
    assert int(f['Duplicate games skipped'])==count-1
    assert int(f['Contributing games'])==1
    assert out.stat().st_size>0
    wall=sum(float(f[k]) for k in ['Processing phase seconds','Deduplication phase wall seconds','Event merge phase seconds','BIN writing phase seconds','BIN validation phase seconds'])
    assert abs(wall-float(f['Seconds']))<0.01
    assert not list(d.glob('.Pgn2BinMT-*'))
    hashes.append(hashlib.sha256(out.read_bytes()).hexdigest())
   assert len(set(hashes))==1
   results.append(dict(games=count,large_tags=large,threads=[1,2,36],passed=True))
(ROOT/'tests/queue-batch-results.json').write_text(json.dumps(results,indent=2)+'\n')
print('PASS: 30 runs; final partial batches, byte-budget batches, identity and phase timings')

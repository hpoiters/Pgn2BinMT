#!/usr/bin/env python3
"""RC2: exact whole-game deduplication, compared with python-chess records."""
import hashlib,json,random,subprocess,tempfile,time
from pathlib import Path
import chess
from test_core import BIN,ROOT,game,expected,records
results=[]
def check(name, sources, unique, threads=6, ply=60, minimum=1, duplicates=0, invalid=0, dedup=True, memory=1):
 if isinstance(sources,str):sources=[sources]
 with tempfile.TemporaryDirectory() as tmp:
  d=Path(tmp);paths=[]
  for i,text in enumerate(sources):
   p=d/f'{i}.pgn';p.write_text(text);paths.append(str(p))
  out=d/'Book.bin';cmd=[str(BIN),'--threads',str(threads),'--ply',str(ply),'--min-games',str(minimum),'--memory-mib',str(memory),'--output',str(out)]
  if not dedup:cmd+=['--no-dedup']
  start=time.perf_counter();r=subprocess.run(cmd+paths,capture_output=True,text=True,timeout=300)
  assert r.returncode==0,(name,r.stdout,r.stderr)
  assert records(out)==expected(unique,ply,minimum),name
  report=(d/'Pgn2BinMT_Report.txt').read_text()
  assert f'Duplicate games skipped: {duplicates}\n' in report,(name,report)
  assert f'Rejected: {invalid}\n' in report,(name,report)
  assert not list(d.glob('.Pgn2BinMT-*'))
  results.append(dict(name=name,passed=True,threads=threads,duplicates=duplicates,seconds=round(time.perf_counter()-start,4),stdout=r.stdout.strip(),sha256=hashlib.sha256(out.read_bytes()).hexdigest()))
  return out.read_bytes()
a=game('1. e4 e5 2. Nf3 Nc6','1-0')
b=game('1. e4 e5 2. Nf3 Nc6','0-1').replace('[Event "Test"]','[Event "Different"]\n[White "Someone"]')
c=game('1. e4 {note} e5 2. Nf3$1 (2. Bc4) Nc6','1/2-1/2')
outputs=[check(f'earliest-result-threads-{t}',a+b+c,a,threads=t,duplicates=2) for t in (1,2,6,18)]
assert len(set(outputs))==1
check('duplicates-do-not-reach-minimum',a*3,a,minimum=3,duplicates=2)
check('disabled-keeps-old-weights',a*3,a*3,minimum=3,dedup=False)
check('across-files-first-valid-copy',[a,b+c],a,duplicates=2)
check('invalid-first-copy-is-discarded',game('1. e4 e5 2. Nf3 Nc6 3. BAD')+a+b,a,duplicates=1,invalid=1)
long_a=game('1. e4 e5 2. Nf3 Nc6 3. Bb5')
long_b=game('1. e4 e5 2. Nf3 Nc6 3. Bc4')
check('different-beyond-book-depth',long_a+long_b+long_a,long_a+long_b,ply=1,duplicates=1)
check('partial-game-is-distinct',a+long_a,a+long_a)
start=chess.STARTING_FEN
explicit=game('1. e4 e5 2. Nf3 Nc6','0-1',start.replace('0 1','19 47'))
check('normal-start-equals-FEN-ignore-clocks',a+explicit,a,duplicates=1)
castle=game('1. 0-0 0-0-0',fen='r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1')
castle2=castle.replace('0-0 0-0-0','O-O O-O-O')
check('castling-spellings',castle+castle2,castle,duplicates=1)
# Identical SAN from different starting boards must remain distinct.
f1=game('1. Nf3',fen='4k3/8/8/8/8/8/8/4K1N1 w - - 0 1')
f2=game('1. Nf3',fen='4k3/8/8/8/8/8/P7/4K1N1 w - - 0 1')
check('different-starts',f1+f2+f1,f1+f2,duplicates=1)
check('demo-default-three-unique',(ROOT/'tests/basic.pgn').read_text(),(ROOT/'tests/basic.pgn').read_text(),minimum=3)
# 25k unique complete games share e4, force original-order halving and several
# external merge passes with tiny buffers. Their extra moves are beyond depth.
random.seed(24680);seen=set();parts=[]
while len(parts)<25000:
 board=chess.Board();board.push_san('e4');moves=['e4']
 for i in range(15):
  if board.is_game_over():break
  move=random.choice(list(board.legal_moves));moves.append(board.san(move));board.push(move)
 key=tuple(moves)
 if key in seen:continue
 seen.add(key);parts.append(game(' '.join(moves),'1-0' if len(parts)<15000 else '0-1'))
unique=''.join(parts);allgames=unique+''.join(reversed(parts))
output=[check(f'external-merges-halving-{t}',allgames,unique,threads=t,ply=1,duplicates=25000) for t in (1,18)]
assert output[0]==output[1]
# Many repeated complete games shrink to three contributors, preserving min=3.
demo=(ROOT/'tests/basic.pgn').read_text()
check('300000-games-bounded-dedup',demo*100000,demo,threads=18,minimum=3,duplicates=299997)
(ROOT/'tests/dedup-results.json').write_text(json.dumps(results,indent=2))
print(f'{len(results)} dedup checks passed')

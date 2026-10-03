#!/usr/bin/env python3
"""Independent expected records via python-chess 1.11.2; classic executable optional."""
import argparse,hashlib,io,json,random,struct,subprocess,tempfile,time
from pathlib import Path
import chess,chess.pgn,chess.polyglot
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/Pgn2BinMT-cli'
RESULTS=[]
def records(path):
 data=path.read_bytes();assert len(data)%16==0
 return {(k,m):(w,l) for k,m,w,l in struct.iter_unpack('>QHHI',data)}
def run(name,pgn,threads=1,ply=60,minimum=1,memory=16,expect=None,success=True):
 with tempfile.TemporaryDirectory() as tmp:
  tmp=Path(tmp);src=tmp/'test.pgn';src.write_text(pgn,encoding='utf-8');out=tmp/'Book.bin'
  start=time.perf_counter();r=subprocess.run([str(BIN),'--no-dedup','--threads',str(threads),'--ply',str(ply),'--min-games',str(minimum),'--memory-mib',str(memory),'--output',str(out),str(src)],capture_output=True,text=True,timeout=300)
  assert (r.returncode==0)==success,(name,r.stdout,r.stderr)
  if not success:assert not out.exists();assert not list(tmp.glob('.Pgn2BinMT-*'));RESULTS.append(dict(name=name,passed=True));return
  assert not list(tmp.glob('.Pgn2BinMT-*'));d=records(out)
  if expect is not None:assert d==expect,(name,len(d),len(expect),list((d.items()-expect.items()))[:4])
  assert all(not learn for weight,learn in d.values());elapsed=time.perf_counter()-start
  RESULTS.append(dict(name=name,passed=True,threads=threads,seconds=round(elapsed,4),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),stdout=r.stdout.strip()))
  return out.read_bytes()
def game(moves,result='1/2-1/2',fen=None):
 tags='[Event "Test"]\n[Result "'+result+'"]\n'
 if fen:tags+='[SetUp "1"]\n[FEN "'+fen+'"]\n'
 return tags+'\n'+moves+' '+result+'\n\n'
def expected(pgn,ply=60,minimum=1):
 stream=io.StringIO(pgn.lstrip('\ufeff'));positions={};gameid=0
 while (g:=chess.pgn.read_game(stream)) is not None:
  gameid+=1;b=g.board();rs=g.headers.get('Result','*')
  for i,m in enumerate(g.mainline_moves()):
   if i>=ply:break
   key=chess.polyglot.zobrist_hash(b);moves=positions.setdefault(key,{})
   target=m.to_square
   if b.is_castling(m):target=chess.square(7 if target>m.from_square else 0,chess.square_rank(m.from_square))
   move=(m.from_square<<6)|target|((m.promotion-1)<<12 if m.promotion else 0)
   s=moves.setdefault(move,[0,0,set()]);s[0]+=1;s[1]+=1 if rs in ('*','1/2-1/2') else 2 if (rs=='1-0')==b.turn else 0;s[2].add(gameid)
   if s[0]>=16384:
    for v in moves.values():v[0]=(v[0]+1)//2;v[1]=(v[1]+1)//2
   b.push(m)
 return {(key,m):(s[1],0) for key,moves in positions.items() for m,s in moves.items() if len(s[2])>=minimum and s[1]}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--classic');ap.add_argument('--million',action='store_true');args=ap.parse_args()
 basic=(ROOT/'tests/classic-basic.pgn').read_text();want=expected(basic)
 data=[run(f'threads-{t}',basic,t,expect=want) for t in (1,2,6,18)];assert len(set(data))==1
 special=game('1. O-O O-O-O',fen='r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1')+game('1. exd6',fen='4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1')
 for piece in ('Q','R','B','N'):special+=game(f'1. a8={piece}',fen='7k/P7/8/8/8/8/8/7K w - - 0 1')
 run('special-moves-FEN',special,expect=expected(special))
 extra=game('1. O-O-O O-O',fen='r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1')+game('1. exd3',fen='4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1')
 for piece in ('Q','R','B','N'):extra+=game(f'1. a1={piece}',fen='7k/8/8/8/8/8/p7/7K b - - 0 1')
 run('black-special-and-both-castles',extra,expect=expected(extra))
 run('nested-RAV-and-attached-NAG',basic.replace('(2. Bc4 Nf6)','(2. Bc4 Nf6 (2... d6))').replace('Nf3 $1','Nf3$1'),expect=want)
 run('unknown-result',game('1. e4 e5','*'),expect=expected(game('1. e4 e5','*')))
 trans=game('1. Nf3 Nf6 2. g3 g6 3. Bg2 Bg7')+game('1. g3 g6 2. Nf3 Nf6 3. Bg2 Bg7')
 run('transpositions',trans,minimum=2,expect=expected(trans,minimum=2))
 for count in (2,3):
  pg=game('1. e4 e5')*count;run(f'minimum-{count}',pg,minimum=3,expect=expected(pg,minimum=3))
 run('BOM-NAG-RAV-semicolon', '\ufeff'+basic.replace('{comment}','{comment\ntext}').replace('e5 2.', 'e5 ; ignore\n2.'),expect=want)
 invalid=game('1. e4 BAD')+game('')+game('1. d4 d5')
 run('invalid-empty-no-partial-events',invalid,expect=expected(game('1. d4 d5')))
 lateinvalid=game('1. e4 e5 2. BAD')+game('1. d4 d5');run('invalid-after-depth',lateinvalid,ply=1,expect=expected(game('1. d4 d5'),1))
 run('unterminated-comment',game('1. e4 { broken'),expect={})
 run('unterminated-comment-recovery',game('1. e4 { broken')+basic,expect=expected(basic))
 repeated=game('1. e4 e5 2. Nf3 Nc6', '1-0')*10000+game('1. d4 d5','0-1')*8000+game('1. e4 e5','1/2-1/2')*10000
 want=expected(repeated)
 for t in (1,2,18):run(f'halving-original-order-{t}',repeated,t,memory=1,expect=want)
 repeated=game('1. Nf3 Nf6 2. Ng1 Ng8 3. Nf3 Nf6')
 run('repetition-distinct-game-minimum',repeated,minimum=2,expect={})
 random.seed(147);pg=''
 for i in range(120):
  b=chess.Board();moves=[]
  for ply in range(75):
   if b.is_game_over():break
   m=random.choice(list(b.legal_moves));moves.append(b.san(m));b.push(m)
  pg+=game(' '.join(moves),random.choice(['1-0','0-1','1/2-1/2']))
 for limit in (1,59,60,61):run(f'depth-random-{limit}',pg,6,ply=limit,expect=expected(pg,limit))
 for fen in ('8/8/8/8/8/8/8/8 w - - 0 1','bad','7k/8/8/8/8/8/8/7K x - - 0 1'):
  run('invalid-FEN',game('1. e4',fen=fen)+basic,expect=expected(basic))
 if args.classic:
  with tempfile.TemporaryDirectory() as tmp:
   tmp=Path(tmp)
   for name,pg in [('basic',basic),('random',pg),('halving',game('1. e4 e5','1-0')*25000)]:
    src=tmp/f'{name}.pgn';out=tmp/f'{name}.bin';src.write_text(pg)
    subprocess.run([args.classic,'make-book','-pgn',str(src),'-bin',str(out),'-max-ply','60','-min-game','1'],check=True,capture_output=True)
    ours=run('classic-'+name,pg,18,memory=1,expect=records(out));assert records(out)=={(k,m):(w,l) for k,m,w,l in struct.iter_unpack('>QHHI',ours)}
 if args.million:
  million=(ROOT/'tests/classic-basic.pgn').read_text().split('[Event "Reference"]')[1];million='[Event "Reference"]'+million
  # 10-million events ensure several external merge passes under 1 MiB buffer.
  pg=million*1000000
  output=[]
  for t in (1,6,18):output.append(run(f'million-games-{t}',pg,t,memory=1))
  assert len(set(output))==1
 report=ROOT/'tests/results.json';report.write_text(json.dumps(RESULTS,indent=2));print(json.dumps(RESULTS,indent=2))
if __name__=='__main__':main()

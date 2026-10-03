"""Deterministic research fixtures; requires chess 1.11.2. Writes under argument directory."""
import random,sys
from pathlib import Path
import chess
from test_core import game
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
(out/'short.pgn').write_text('[Event "Queue benchmark"]\n[Result "1-0"]\n\n1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1-0\n\n'*200000)
random.seed(147);games=[]
for i in range(120):
 b=chess.Board();moves=[]
 for ply in range(75):
  if b.is_game_over():break
  m=random.choice(list(b.legal_moves));moves.append(b.san(m));b.push(m)
 games.append(game(' '.join(moves),random.choice(['1-0','0-1','1/2-1/2'])))
(out/'benchmark.pgn').write_text(''.join(games)*300)
random.seed(629)
with (out/'unique.pgn').open('w') as f:
 for i in range(6000):
  b=chess.Board();moves=[]
  for ply in range(60):
   if b.is_game_over():break
   m=random.choice(list(b.legal_moves));moves.append(b.san(m));b.push(m)
  f.write('[Event "Unique stress"]\n[Result "1-0"]\n\n'+' '.join(moves)+' 1-0\n\n')

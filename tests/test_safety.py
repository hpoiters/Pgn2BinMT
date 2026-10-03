import tempfile,subprocess,time,signal,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BIN=ROOT/'build/Pgn2BinMT-cli';res=[]
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);src=d/'games.pgn';src.write_bytes((ROOT/'tests/basic.pgn').read_bytes()*200000);out=d/'Book.bin';out.write_bytes(b'ORIGINAL')
 cmd=[str(BIN),'--threads','6','--output',str(out),str(src)]
 p=subprocess.run(cmd,capture_output=True,text=True);assert p.returncode and out.read_bytes()==b'ORIGINAL';res.append('Existing BIN preserved without overwrite authorization')
 p=subprocess.Popen(cmd+['--overwrite'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);time.sleep(.15);p.send_signal(signal.SIGINT);stdout,stderr=p.communicate(timeout=20)
 assert p.returncode and out.read_bytes()==b'ORIGINAL',(stdout,stderr)
 assert not list(d.glob('.Pgn2BinMT-*'));assert (d/'Pgn2BinMT_Incomplete.txt').exists();res.append('Controlled stop preserves existing BIN and removes temporary files')
 p=subprocess.run([str(BIN),'--output'],capture_output=True,text=True);assert p.returncode;res.append('Missing option value handled')
ROOT.joinpath('tests/safety-results.json').write_text(json.dumps(res,indent=2));print(res)

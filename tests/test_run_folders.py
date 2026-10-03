import hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BIN=ROOT/'build/Pgn2BinMT-cli'
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);base=d/'OUTPUT';base.mkdir();out=base/'Book.bin';out.write_bytes(b'ORIGINAL')
 source=ROOT/'tests/basic.pgn'
 command=[str(BIN),'--threads','6','--output',str(out)]
 snapshots={};folders=[]
 for _ in range(3):
  p=subprocess.run(command+['--run-folder',str(source)],capture_output=True,text=True,check=True)
  assert p.stdout.startswith('Process ended: OK ')
  current=list(base.glob('Run_*'));assert len(current)==len(folders)+1
  for path,contents in snapshots.items():assert path.read_bytes()==contents
  new=next(path for path in current if path not in folders);folders.append(new)
  assert len((new/'Book.bin').read_bytes())==160
  assert (new/'Pgn2BinMT_Report.txt').exists() and (new/'Pgn2BinMT_Errors.txt').read_bytes()==b''
  for path in new.iterdir():snapshots[path]=path.read_bytes()
  assert out.read_bytes()==b'ORIGINAL'
 # Exact-path CLI mode allows isolated validation of repeated report copying.
 report=base/'Pgn2BinMT_Report.txt';errors=base/'Pgn2BinMT_Errors.txt';report.write_bytes(b'OLD');errors.write_bytes(b'OLD')
 bad=d/'bad.pgn';bad.write_text('[Event "Bad"]\n[Result "*"]\n\n1. BAD *\n\n'+source.read_text())
 subprocess.run(command+['--overwrite',str(bad)],check=True,capture_output=True)
 assert b'Illegal SAN' in errors.read_bytes() and b'Pgn2BinMT 2.0' in report.read_bytes()
 subprocess.run(command+['--overwrite',str(source)],check=True,capture_output=True)
 assert errors.read_bytes()==b''
 assert not list(base.glob('.Pgn2BinMT-*'))
 results={'passed':True,'distinct_run_folders':len(folders),'earlier_files_preserved':True,'base_bin_preserved_by_run_folder_mode':True,'nonempty_and_empty_reports_overwritten':True,'bin_bytes':160}
 (ROOT/'tests/run-folder-results.json').write_text(json.dumps(results,indent=2)+'\n');print(results)

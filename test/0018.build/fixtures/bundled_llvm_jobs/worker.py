import fcntl,json,sys,time
from pathlib import Path
state=Path(sys.argv[1]);out=Path(sys.argv[2]);state.parent.mkdir(parents=True,exist_ok=True)
def update(delta):
 with state.open('a+b') as f:
  fcntl.flock(f,fcntl.LOCK_EX);f.seek(0);raw=f.read()
  obj=json.loads(raw) if raw else dict(current=0,peak=0,started=0,finished=0)
  obj['current']+=delta;obj['peak']=max(obj['peak'],obj['current'])
  obj['started' if delta>0 else 'finished']+=1
  f.seek(0);f.truncate();f.write(json.dumps(obj).encode());f.flush()
update(1)
time.sleep(.18)
update(-1)
out.parent.mkdir(parents=True,exist_ok=True);out.write_text('done')

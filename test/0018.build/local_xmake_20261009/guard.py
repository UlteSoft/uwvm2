import ctypes as C, fcntl, json, os, signal, subprocess, sys, time
from pathlib import Path
sys.path.insert(0, '/Users/liyinan/Documents/MacroModel/src/uwvm2/test/0017.runtime')
import macos_owned_debug_process_rss as native
native.MAX_MEMBERS = 128
kernel = native.MacNative()
out = Path(sys.argv[1]); command = sys.argv[2:]
out.parent.mkdir(parents=True, exist_ok=True)
seen = {}
kernel.proc.proc_listallpids.argtypes = (C.c_void_p, C.c_int)
kernel.proc.proc_listallpids.restype = C.c_int
def owned_members(root):
 text = subprocess.check_output(['/bin/ps', '-axo', 'pid=,ppid='], text=True)
 parents = {int(parts[0]):int(parts[1]) for line in text.splitlines() if len(parts := line.split()) == 2}
 owned = {root, os.getpid()}
 previous = set()
 while owned != previous:
  previous = owned.copy()
  owned |= {pid for pid,parent in parents.items() if parent in owned}
 for pid,birth in seen.items():
  if pid in parents:
   try: bsd = kernel.bsd(pid, missing=True)
   except RuntimeError:
    # A historical compiler PID may now belong to an inaccessible foreign
    # process after PID wraparound. It is not an owned descendant in this
    # snapshot and must never be signalled based on the old PID alone.
    continue
   if bsd is not None and kernel.birth(bsd) == birth: owned.add(pid)
 result = []
 for pid in owned:
  bsd = kernel.bsd(pid, missing=True)
  if bsd is not None:
   seen[pid] = kernel.birth(bsd); result.append((pid,bsd))
 return result
def safe_rss(pid, bsd):
 for attempt in range(8):
  try: return kernel.rss(pid, bsd)
  except RuntimeError:
   time.sleep(0.002)
   current = kernel.bsd(pid, missing=True)
   if current is None or current.status == 5: return 0
   if kernel.birth(current) != kernel.birth(bsd): return 0
   bsd = current
 raise RuntimeError('live RSS accounting remained unavailable')
lease = (out.parent / 'serial-build.lock').open('a')
fcntl.flock(lease, fcntl.LOCK_EX)
peak = 0; stopped = None; pid_peak = {}; start = time.monotonic()
with out.with_suffix('.log').open('wb') as log:
 child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
 try:
  while child.poll() is None:
   rows = []
   for pid, bsd in owned_members(child.pid):
    rss = safe_rss(pid, bsd); rows.append((pid, rss))
    pid_peak[str(pid)] = max(pid_peak.get(str(pid), 0), rss)
   total = sum(rss for _, rss in rows)
   peak = max(peak, total)
   if total >= (1792 << 20): stopped = 'aggregate RSS early stop before 2 GiB'; break
   if os.statvfs(out.parent).f_bavail * os.statvfs(out.parent).f_frsize < (1024 << 20): stopped = 'disk reserve 1 GiB'; break
   if out.with_suffix('.log').stat().st_size > (16 << 20): stopped = 'bounded log 16 MiB'; break
   time.sleep(0.025)
 except Exception as error:
  stopped = 'supervisor accounting error: ' + str(error)
 finally:
  if stopped or child.poll() is None:
   for pid, bsd in owned_members(child.pid):
    if pid != os.getpid() and bsd.status != 5:
     try: os.kill(pid, signal.SIGTERM)
     except (ProcessLookupError, PermissionError): pass
   time.sleep(0.2)
   for pid, bsd in owned_members(child.pid):
    if pid != os.getpid() and bsd.status != 5:
     try: os.kill(pid, signal.SIGKILL)
     except (ProcessLookupError, PermissionError): pass
  code = child.wait()
  for _ in range(100):
   live = [pid for pid, bsd in owned_members(child.pid) if pid != os.getpid() and bsd.status != 5]
   if not live: break
   time.sleep(0.025)
  if live: raise RuntimeError(('owned children remain', live))
receipt = dict(command=command,cwd=os.getcwd(),returncode=code,stop_reason=stopped,sampled_aggregate_peak_bytes=peak,seconds=time.monotonic()-start,per_pid_sampled_peak_bytes=pid_peak,live_children_remaining=live,scope='RSS observations with conservative early stop; not a kernel memory cap')
out.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({k:v for k,v in receipt.items() if k != 'per_pid_sampled_peak_bytes'},indent=2))
print(out.with_suffix('.log').read_text(errors='replace')[-12000:])
sys.exit(0 if code == 0 and stopped is None else 1)

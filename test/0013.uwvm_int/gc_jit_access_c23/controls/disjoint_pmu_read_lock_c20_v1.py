from pathlib import Path
import os,json,time,re
import native as N
original=N.processes
journal=[]
def args(pid):
 return (Path('/proc')/str(pid)/'cmdline').read_bytes().decode().strip('\0').split('\0')
def ppid(pid):
 s=(Path('/proc')/str(pid)/'stat').read_text();return int(s[s.rfind(')')+2:].split()[1])
def cpus(v):
 out=set()
 for s in v.split(','):
  if '-' in s:
   lo,hi=map(int,s.split('-'));assert 0<=lo<=hi<=31;out.update(range(lo,hi+1))
  else:out.add(int(s))
 assert out and out<=set(range(32));return out
reserved=cpus(Path('/sys/devices/system/cpu/cpu2/topology/thread_siblings_list').read_text().strip())
reserved.add(16) # hybrid-PMU companion in isolated feature specs
assert 2 in reserved
def classify(d):
 try:
  a=args(d['pid'])
  if d['comm']=='vtune' and '-report' in a and not any(x in a for x in ['-collect','-collect-with']):return {'reason':'report only','pid':d['pid'],'argv':a}
  if d['comm']=='vtune' and '-command' in a and not any(x in a for x in ['-collect','-collect-with']):
   pos=a.index('-command')
   if pos+1<len(a) and a[pos+1] in {'status','resume','pause','stop'}:return {'reason':'VTune collection control command; actual collector scope checked separately','pid':d['pid'],'argv':a}
  if d['comm']=='amplxe-runss' and '--command' in a:
   # Only a complete control-only argument grammar; actual sampler is checked separately.
   pairs={'--ui-output-format','--ui-output-fd','-r','--command'}
   rest=a[1:]
   if len(rest)%2==0 and all(rest[i] in pairs for i in range(0,len(rest),2)) and len({rest[i] for i in range(0,len(rest),2)})==len(rest)//2:
    pos=a.index('--command')
    if a[pos+1] in {'status','pause','resume','stop'}:return {'reason':'runss control-only command grammar; actual collector checked separately','pid':d['pid'],'argv':a}
  if d['comm']=='perf' or d['comm']=='amplxe-perf':
   if 'report' in a and 'record' not in a and 'stat' not in a:return {'reason':'perf report only','pid':d['pid'],'argv':a}
   if not any(v in a for v in ['record','stat']) or '-C' not in a:return None
   mask=cpus(a[a.index('-C')+1]);events=[a[i+1] for i in range(len(a)-1) if a[i]=='-e']
   if not events or any('uncore' in e or not set(re.findall(r'([A-Za-z_][A-Za-z0-9_]*)/',e)) or not set(re.findall(r'([A-Za-z_][A-Za-z0-9_]*)/',e))<={'cpu_core','cpu_atom'} for e in events):return None
   if mask&reserved:return None
   return {'reason':'driverless core PMU on disjoint explicit CPUs','pid':d['pid'],'cpus':sorted(mask),'reserved':sorted(reserved),'argv':a}
  pid=d['pid']
  for _ in range(8):
   a=args(pid)
   if Path(a[0]).name=='vtune':break
   pid=ppid(pid)
  else:return None
  if '-collect' in a and a[a.index('-collect')+1]=='uarch-exploration':
   if '-cpu-mask' not in a:return None
   mask=cpus(a[a.index('-cpu-mask')+1])
   if mask&reserved:return None
   if '-result-dir' not in a:return None
   opt=Path(a[a.index('-result-dir')+1])/'config/runsa.options'
   # Resolve a transient launch before declaring scope unknown. This is a
   # bounded config-file check, never a CPU-utilization/idle wait or exemption.
   if not opt.exists() and 'pmu-collection-mode=summary' in a:
    deadline=time.monotonic()+.5
    while not opt.exists() and time.monotonic()<deadline:time.sleep(.02)
   text=opt.read_text() if opt.exists() else ''
   if text and ('--collector=perf' not in text or 'UNC_' in text or 'uncore_' in text):return None
   if not text and 'dram-bandwidth-limits=false' not in a:return None
   return {'reason':'disjoint explicit CPU mask for uarch with DRAM collection disabled','pid':d['pid'],'controller':pid,'cpus':sorted(mask),'reserved':sorted(reserved),'argv':a,'options_sha':N.sha(opt) if opt.exists() else None,'backend_confirmed':bool(text)}
  if '-collect-with' not in a or a[a.index('-collect-with')+1]!='runsa':return None
  if '-cpu-mask' not in a or 'enable-driverless-collection=true' not in a:return None
  masks=cpus(a[a.index('-cpu-mask')+1]);ev=[x.split('=',1)[1] for x in a if x.startswith('event-config=')]
  if len(ev)!=1 or masks&reserved:return None
  if not all(x.split(':',1)[0] in {'CPU_CLK_UNHALTED.REF_TSC','CPU_CLK_UNHALTED.CORE','INST_RETIRED.ANY'} for x in ev[0].split(',')):return None
  return {'reason':'disjoint custom driverless core PMU controller','pid':d['pid'],'controller':pid,'cpus':sorted(masks),'reserved':sorted(reserved),'argv':a}
 except (OSError,ValueError,IndexError,AssertionError):
  if not (Path('/proc')/str(d['pid'])).exists():return {'reason':'collector exited before scope observation; no live PMU holder','pid':d['pid']}
  return None
def scoped(pgid=None):
 owned,foreign=original(pgid);deny=[];allow=[]
 for d in foreign:
  result=classify(d)
  if result is None:
   try:observed_argv=args(d['pid'])
   except (OSError,ValueError):observed_argv=[]
   deny.append(dict(d,observed_argv=observed_argv))
  else:allow.append(result)
 if allow or deny:
  entry={'time':time.time(),'pgid':pgid,'allowed':allow,'denied':deny}
  if not journal or ([(d['pid'],d['reason']) for d in allow],deny)!=([(d['pid'],d['reason']) for d in journal[-1]['allowed']],journal[-1]['denied']):
   journal.append(entry);N.latest(N.ROOT/'evidence/disjoint-PMU-observations-c20-v7.json',{'reserved_cpus':sorted(reserved),'observations':journal[-256:],'policy':'no CPU utilization gate; only report processes or explicitly CPU-scoped driverless core PMU collections on CPUs disjoint from CPU2 and its SMT siblings are allowed; unknown/overlapping/uncore collectors stop only owned collection; sample-rate decrease guard remains','native_guard_sha':N.sha(N.ROOT/'control/native.py'),'wrapper_sha':N.sha(__file__)})
 return owned,deny
N.processes=scoped

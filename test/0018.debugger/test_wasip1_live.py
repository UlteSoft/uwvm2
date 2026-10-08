#!/usr/bin/env python3
"""Actual CLI, authenticated broker and DAP WASIp1 integration; run inside the test cgroup."""
import sys,os,json,hashlib,subprocess,time,selectors,re,traceback,gzip,importlib.util,io
from pathlib import Path
from process_shutdown import wait_for_normal_exit
S=Path(__file__).resolve().parents[2]
if len(sys.argv)!=4:raise SystemExit('usage: test_wasip1_live.py UWVM WASM_TOOLS OUTPUT_DIRECTORY')
PRODUCT=Path(sys.argv[1]).resolve();W=Path(sys.argv[2]).resolve();TD=Path(sys.argv[3]).resolve();TD.mkdir(parents=True,exist_ok=True)
if sys.platform!='linux':raise SystemExit('This integration fixture requires the authenticated Linux broker.')
subprocess.run(['bash',str(S/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
rows=[]
def record(name,good,**data):
 rows.append(dict(name=name,passed=bool(good),**data));(TD/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps({'name':name,'passed':bool(good),'error':data.get('error')}),flush=True)
def run(name,argv,timeout=180,expected=0):
 log=TD/(name+'.log');wrapper=[sys.executable,'-c','import os,sys,time;time.sleep(.25);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)',*map(str,argv)]
 with log.open('wb') as f:c=subprocess.run(wrapper,stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 record(name,c.returncode==expected,argv=list(map(str,argv)),exit=c.returncode,log=str(log));return c.returncode==expected
class Console:
 def __init__(self,policy,extra=(),name='cli',fixture='cli.wasm'):
  self.name=name+'-'+policy;self.log=bytearray();self.pending=bytearray();self.commands=[]
  self.argv=[str(PRODUCT),'-Rdbg','-Rct','0','-Rllvm-call-stack',policy,'-Rllvm-cache-path','disable',
   '-WFE-gc','-WFE-function-references','--wasip1-global-noinherit-system-environment','--wasip1-global-force-args','2','argv0','before',
   '--wasip1-global-add-or-replace-environment','UWVM_DEBUG_KEY','old',*map(str,extra),'--run',str(TD/fixture)]
  self.child=subprocess.Popen([sys.executable,'-c','import os,sys,time;time.sleep(.15);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)',*self.argv],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  self.sel=selectors.DefaultSelector();self.sel.register(self.child.stdout,selectors.EVENT_READ)
  try:self.prompt()
  except BaseException:
   self.close();raise
 def prompt(self):
  end=time.monotonic()+30; marker=b'(uwvm-debug) '
  while marker not in self.pending:
   assert time.monotonic()<end,(self.name,'prompt timeout',bytes(self.log)[-3000:])
   assert self.sel.select(max(0,end-time.monotonic())),(self.name,'prompt timeout')
   data=os.read(self.child.stdout.fileno(),65536);assert data,(self.name,'early exit',self.child.poll(),bytes(self.log)[-3000:])
   self.log.extend(data);self.pending.extend(data)
  pos=self.pending.index(marker)+len(marker);data=bytes(self.pending[:pos]);del self.pending[:pos];return data.decode('utf-8','replace')
 def send(self,text):
  self.commands.append(text);self.child.stdin.write(text.encode()+b'\n');self.child.stdin.flush();return self.prompt()
 def until(self,marker):
  end=time.monotonic()+20
  while True:
   text=self.send('status')
   if marker in text:return text
   assert time.monotonic()<end,('state timeout',marker,text)
   time.sleep(.02)
 def close(self):
  if self.child.poll() is None:
   try:self.child.stdin.write(b'quit\n');self.child.stdin.flush()
   except BrokenPipeError:pass
   try:self.child.stdin.close()
   except BrokenPipeError:pass
  shutdown=wait_for_normal_exit(self.child,10)
  record(self.name+'-managed-close',shutdown.pop('passed'),**shutdown)
  self.log.extend(self.child.stdout.read());self.sel.close()
  (TD/(self.name+'.log')).write_bytes(self.log)
  (TD/(self.name+'-commands.json')).write_text(json.dumps(self.commands,indent=2)+'\n')
def ok(text):return 'status=ok ' in text
def checked(c,name,command,predicate):
 text=c.send(command);record(c.name+'-'+name,predicate(text),command=command,reply=text);return text
def prepare_wat():
 original=(S/'test/0017.runtime/fixtures/debug_wasip1_environment.wat').read_text()
 original=original.replace('(func (export "run")','(func $run (export "run")')
 at=original.rfind(')')
 original=original[:at]+'\n(func (export "_start") call $run i32.const 91 i32.ne if unreachable end)\n'+original[at:]
 (TD/'cli.wat').write_text(original)
 assert run('wat-cli-parse',[W,'parse',TD/'cli.wat','-o',TD/'cli.wasm'])
 assert run('wat-cli-validate',[W,'validate',TD/'cli.wasm','--features','all'])
if True:
 prepare_wat();(TD/'guestdir').mkdir(exist_ok=True)
 for policy in ['instruction','unwind']:
  c=None
  try:
   c=Console(policy,['--wasip1-global-mount-dir','/sandbox',TD/'guestdir','--wasip1-global-trace','file',TD/('trace-'+policy+'.log')])
   checked(c,'help','help',lambda x:'info wasip1 args|env|fds|preopens' in x)
   checked(c,'prepared-refusal','info wasip1 args 0',lambda x:not ok(x))
   checked(c,'nop-break','break 0 4 7',lambda x:'breakpoint' in x and 'error:' not in x)
   c.send('continue');stopped=c.until('stopped: breakpoint')
   record(c.name+'-actual-stop',True,reply=stopped)
   checked(c,'args','info wasip1 args 0',lambda x:ok(x) and '"before"' in x and '"argv0"' in x)
   checked(c,'args-page','info wasip1 args 0 0 1',lambda x:ok(x) and 'more next=1' in x)
   checked(c,'args-page-next','info wasip1 args 0 1 1',lambda x:ok(x) and '[1] "before"' in x and 'more next=' not in x)
   checked(c,'env','info wasip1 env 0',lambda x:ok(x) and '"UWVM_DEBUG_KEY=old"' in x)
   fdtext=checked(c,'fds','info wasip1 fds 0',lambda x:ok(x) and all('fd='+str(f)+' ' in x for f in [1,2]))
   checked(c,'preopens','info wasip1 preopens 0',lambda x:ok(x) and 'preopened=1 guest-name="/sandbox"' in x)
   checked(c,'invalid-module','info wasip1 env 999999',lambda x:not ok(x) and 'applied=0' in x)
   checked(c,'invalid-arg-index','set wasip1 arg 0 4095 61',lambda x:'entry not found' in x and 'applied=0' in x)
   checked(c,'empty-arg','set wasip1 arg 0 1 -',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'empty-arg-read','info wasip1 args 0',lambda x:ok(x) and '[1] ""' in x)
   stop_id=int(re.search(r'stop-id ([0-9]+)',stopped)[1])
   checked(c,'guarded-stale-refusal',f'set wasip1 arg 0 1 77726f6e67 if-stop {stop_id+1}',lambda x:'stale stop or generation' in x and 'applied=0' in x)
   checked(c,'guarded-no-commit','info wasip1 args 0',lambda x:'[1] ""' in x)
   checked(c,'edit-arg',f'set wasip1 arg 0 1 6166746572 if-stop {stop_id}',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'edit-env','set wasip1 env 0 5557564d5f44454255475f4b4559 6e6577',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'add-env','set wasip1 env 0 4558545241 74656d70',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'add-env-read','info wasip1 env 0',lambda x:ok(x) and '"EXTRA=temp"' in x)
   checked(c,'remove-env','unset wasip1 env 0 4558545241',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'remove-missing','unset wasip1 env 0 4558545241',lambda x:'entry not found' in x and 'applied=0' in x)
   checked(c,'empty-env','set wasip1 env 0 454d505459 -',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'empty-env-read','info wasip1 env 0',lambda x:ok(x) and '"EMPTY="' in x)
   checked(c,'remove-empty','unset wasip1 env 0 454d505459',lambda x:ok(x) and 'applied=1' in x)
   for name,command in [('nul','set wasip1 arg 0 1 00'),('key-equals','set wasip1 env 0 413d 62'),('page-zero','info wasip1 env 0 0 0'),('page-65','info wasip1 env 0 0 65')]:
    checked(c,'reject-'+name,command,lambda x:'error:' in x and not ok(x))
   match=re.search(r'fd=2 storage-kind=.*?rights-base=(0x[0-9a-f]+) rights-inheriting=(0x[0-9a-f]+)',fdtext);assert match,fdtext
   base_rights,inherit=map(lambda x:int(x,16),match.groups())
   checked(c,'hex-rights-input','set wasip1 rights 0 2 '+match[1]+' '+match[2]+' '+match[1]+' '+match[2],lambda x:ok(x) and 'applied=1' in x)
   checked(c,'stale-rights',f'set wasip1 rights 0 2 {base_rights^1} {inherit} 0 0',lambda x:'descriptor rights changed' in x and 'applied=0' in x)
   checked(c,'rights-escalation',f'set wasip1 rights 0 2 {base_rights} {inherit} {base_rights|(1<<63)} {inherit}',lambda x:'cannot increase descriptor capabilities' in x and 'applied=0' in x)
   checked(c,'rights-reduce',f'set wasip1 rights 0 2 {base_rights} {inherit} 0 0',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'rights-read','info wasip1 fds 0 2 1',lambda x:ok(x) and 'fd=2 ' in x and 'rights-base=0x0 rights-inheriting=0x0' in x)
   checked(c,'bad-fd','set wasip1 rights 0 99999 0 0 0 0',lambda x:'guest descriptor unavailable' in x and 'applied=0' in x)
   checked(c,'insert-middle','set wasip1 arg-insert 0 1 6d6964646c65',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'insert-middle-read','info wasip1 args 0',lambda x:'[1] "middle"' in x and '[2] "after"' in x)
   checked(c,'remove-middle','unset wasip1 arg 0 1',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'long-4096','set wasip1 arg 0 1 '+'7a'*4096,lambda x:ok(x) and 'applied=1' in x)
   checked(c,'long-read','info wasip1 args 0 1 1',lambda x:ok(x) and 'z'*4096 in x)
   checked(c,'restore-after','set wasip1 arg 0 1 6166746572',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'script-edit','wasm-script set wasip1 arg 0 1 6166746572;info wasip1 args 0',lambda x:'wasm-script end' in x and '[1] "after"' in x)
   baseline=checked(c,'checkpoint-save-baseline','set wasip1 checkpoint 0 0',lambda x:ok(x) and 'checkpoint Wasm and WASIp1 together' in x)
   retained=int(re.search(r'retained-external=([0-9]+)',baseline)[1])
   portable=TD/('portable-'+policy+'.uwp');portable_hex=str(portable).encode().hex()
   checked(c,'portable-export',f'set wasip1 export 0 {portable_hex}',lambda x:ok(x) and 'wasip1-portable resources=' in x and 'checkpoint Wasm and WASIp1 together' in x)
   digest=hashlib.sha256(portable.read_bytes()).hexdigest()
   checked(c,'portable-exclusive-refusal',f'set wasip1 export 0 {portable_hex}',lambda x:'native resource operation failed' in x and 'applied=0' in x)
   record(c.name+'-portable-exclusive-preserves',hashlib.sha256(portable.read_bytes()).hexdigest()==digest)
   checked(c,'portable-stale-refusal',f'set wasip1 import 0 {portable_hex} if-stop {stop_id+1}',lambda x:'stale stop or generation' in x and 'applied=0' in x)
   broken=TD/('portable-broken-'+policy+'.uwp');data=bytearray(portable.read_bytes());data[-1]^=1;broken.write_bytes(data)
   # Persisted DATA cannot change the target debugger's reserved FD0 kind.
   reserved=bytearray(portable.read_bytes());counts=[int.from_bytes(reserved[100+4*i:104+4*i],'little') for i in range(6)];pos=124
   for _ in range(counts[0]+counts[1]):n=int.from_bytes(reserved[pos:pos+4],'little');pos+=4+n
   for _ in range(counts[2]):
    pos+=16
    for _ in range(2):n=int.from_bytes(reserved[pos:pos+4],'little');pos+=4+n
   pos+=24*counts[3];assert counts[4]>0 and int.from_bytes(reserved[pos:pos+4],'little')==0
   reserved[pos+20]^=1;reserved[-32:]=hashlib.sha256(reserved[:-32]).digest();reserved_path=TD/('portable-reserved-'+policy+'.uwp');reserved_path.write_bytes(reserved)
   checked(c,'portable-reserved-refusal','set wasip1 import 0 '+str(reserved_path).encode().hex(),lambda x:'target reserved descriptor must remain reserved'.replace(' ',r'\x20') in x and 'applied=0' in x)
   checked(c,'portable-reserved-no-commit','info wasip1 args 0',lambda x:'"after"' in x)
   fifo=TD/('portable-fifo-'+policy);os.mkfifo(fifo)
   checked(c,'portable-fifo-refusal','set wasip1 import 0 '+str(fifo).encode().hex(),lambda x:'invalid request' in x and 'applied=0' in x)
   checked(c,'portable-edit-before-refusal','set wasip1 arg 0 1 77726f6e67',ok)
   checked(c,'portable-digest-refusal','set wasip1 import 0 '+str(broken).encode().hex(),lambda x:'invalid request' in x and 'applied=0' in x)
   checked(c,'portable-failure-no-commit','info wasip1 args 0',lambda x:'"wrong"' in x)
   checked(c,'portable-import',f'set wasip1 import 0 {portable_hex}',lambda x:ok(x) and 'applied=1' in x and 'content=external' in x)
   checked(c,'portable-restored-argv','info wasip1 args 0',lambda x:'"after"' in x)

   created=checked(c,'managed-create-binary','set wasip1 file 0 410042',lambda x:ok(x) and 'wasip1-fd affected=' in x)
   fd=int(re.search(r'affected=([0-9]+)',created)[1])
   copied=checked(c,'managed-duplicate',f'set wasip1 fd-dup 0 {fd} 0x60006e 0',lambda x:ok(x) and 'applied=1' in x)
   alias=int(re.search(r'affected=([0-9]+)',copied)[1])
   checked(c,'checkpoint-save-managed','set wasip1 checkpoint 0 1',lambda x:ok(x) and f'managed=1 retained-external={retained}' in x)
   checked(c,'checkpoint-invalid-slot','set wasip1 checkpoint 0 8',lambda x:'error:' in x)
   checked(c,'checkpoint-explicit-restore-mode','set wasip1 restore 0 1',lambda x:'error:' in x)
   checked(c,'checkpoint-stale-guard',f'set wasip1 restore 0 1 bindings if-stop {stop_id+1}',lambda x:'stale stop or generation' in x and 'applied=0' in x)
   checked(c,'checkpoint-edit-after-save','set wasip1 arg 0 1 77726f6e67',lambda x:ok(x))
   checked(c,'checkpoint-strict-refusal','set wasip1 restore 0 1 strict',lambda x:'resource rollback unavailable' in x and 'applied=0' in x)
   checked(c,'checkpoint-strict-no-commit','info wasip1 args 0',lambda x:'"wrong"' in x)
   for number in [fd,alias]:
    checked(c,'managed-close-'+str(number),f'unset wasip1 fd 0 {number} 0x60006e 0',lambda x:ok(x) and 'applied=1' in x)
   checked(c,'managed-reuse','set wasip1 file 0 7265706c61636564',lambda x:ok(x))
   checked(c,'checkpoint-restore-reused-fds','set wasip1 restore 0 1 bindings',lambda x:ok(x) and 'managed=1' in x)
   checked(c,'checkpoint-restored-argv','info wasip1 args 0',lambda x:'"after"' in x)
   checked(c,'checkpoint-restored-alias-bindings','info wasip1 fds 0',lambda x:all('fd='+str(n)+' ' in x for n in [fd,alias]) and 'fd=0 ' not in x)
   checked(c,'checkpoint-empty-slot','set wasip1 restore 0 7 bindings',lambda x:'entry not found' in x and 'applied=0' in x)
   checked(c,'checkpoint-baseline-removes-managed','set wasip1 restore 0 0 bindings',lambda x:ok(x) and 'managed=0' in x)
   checked(c,'checkpoint-drop-managed','unset wasip1 checkpoint 0 1',lambda x:ok(x))
   checked(c,'checkpoint-drop-baseline','unset wasip1 checkpoint 0 0',lambda x:ok(x))
   checked(c,'dynamic-trace','trace wasip1 on all',lambda x:'wasip1-trace enabled=1 filter=all' in x)
   c.send('delete 1');c.send('continue');ended=c.until('guest exited:')
   record(c.name+'-guest-reads-edits','guest exited: 0' in ended,reply=ended)
   calls=checked(c,'dynamic-trace-read','trace wasip1 read 0 64',lambda x:all('name='+name in x for name in ['args_sizes_get','args_get','environ_sizes_get','environ_get']) and x.count('phase=entry')==4 and x.count('phase=return')==4 and x.count('errno=0 errno-name=esuccess')==4)
   record(c.name+'-entry-return-pairs',len(re.findall(r'call=([0-9]+) phase=entry',calls))==4 and sorted(re.findall(r'call=([0-9]+) phase=entry',calls))==sorted(re.findall(r'call=([0-9]+) phase=return',calls)))
   checked(c,'exited-refusal' ,'info wasip1 env 0',lambda x:not ok(x))
  except Exception as e:record('cli-'+policy+'-exception',False,error=repr(e),trace=traceback.format_exc())
  finally:
   if c is not None:c.close()

  fresh=None
  try:
   target=TD/('fresh-mount-'+policy);target.mkdir(exist_ok=True)
   fresh=Console(policy,['--wasip1-global-mount-dir','/sandbox',target],name='portable-fresh')
   fresh.send('break 0 4 7');fresh.send('continue');fresh.until('stopped: breakpoint')
   # A reduced alias must not hide the original capability on the same RC.
   fdtext=checked(fresh,'fresh-alias-fds','info wasip1 fds 0',ok)
   for descriptor in (3,1):
    matched=re.search(r'fd='+str(descriptor)+r' storage-kind=.*?rights-base=(0x[0-9a-f]+) rights-inheriting=(0x[0-9a-f]+)',fdtext);assert matched,fdtext
    base,inherit=matched.groups()
    duplicated=checked(fresh,'fresh-alias-duplicate-'+str(descriptor),f'set wasip1 fd-dup 0 {descriptor} {base} {inherit}',lambda x:ok(x) and 'applied=1' in x)
    alias=int(re.search(r'wasip1-fd affected=([0-9]+)',duplicated)[1])
    checked(fresh,'fresh-alias-reduce-'+str(descriptor),f'set wasip1 rights 0 {alias} {base} {inherit} 0 0',lambda x:ok(x) and 'applied=1' in x)
   cp=TD/('portable-'+policy+'.uwp')
   checked(fresh,'fresh-import','set wasip1 import 0 '+str(cp).encode().hex(),lambda x:ok(x) and 'applied=1' in x)
   checked(fresh,'fresh-argv','info wasip1 args 0',lambda x:'"after"' in x)
   checked(fresh,'fresh-env','info wasip1 env 0',lambda x:'"UWVM_DEBUG_KEY=new"' in x)
   checked(fresh,'fresh-target-mount','info wasip1 preopens 0',lambda x:'guest-name="/sandbox"' in x)
   # No one target FD grants the full saved stdout mask. The consumer must
   # refuse, even though the union of two aliases would cover it.
   fdtext=fresh.send('info wasip1 fds 0')
   matched=re.search(r'fd=1 storage-kind=.*?rights-base=(0x[0-9a-f]+) rights-inheriting=(0x[0-9a-f]+)',fdtext);assert matched,fdtext
   base,inherit=map(lambda x:int(x,16),matched.groups());part=base & -base;assert base!=part
   duplicated=checked(fresh,'fresh-split-duplicate',f'set wasip1 fd-dup 0 1 {base} {inherit}',lambda x:ok(x) and 'applied=1' in x)
   alias=int(re.search(r'wasip1-fd affected=([0-9]+)',duplicated)[1])
   checked(fresh,'fresh-split-original',f'set wasip1 rights 0 1 {base} {inherit} {part} 0',lambda x:ok(x) and 'applied=1' in x)
   checked(fresh,'fresh-split-alias',f'set wasip1 rights 0 {alias} {base} {inherit} {base^part} 0',lambda x:ok(x) and 'applied=1' in x)
   checked(fresh,'fresh-split-refusal','set wasip1 import 0 '+str(cp).encode().hex(),lambda x:'target stdio rights insufficient'.replace(' ',r'\x20') in x and 'applied=0' in x)
   checked(fresh,'fresh-split-no-commit','info wasip1 args 0',lambda x:'"after"' in x)
   fresh.send('delete 1');fresh.send('continue');ended=fresh.until('guest exited:')
   record(fresh.name+'-fresh-resume', 'guest exited: 0' in ended,reply=ended)
  except Exception as e:record('portable-fresh-'+policy+'-exception',False,error=repr(e),trace=traceback.format_exc())
  finally:
   if fresh is not None:fresh.close()
  trace=TD/('trace-'+policy+'.log')
  text=trace.read_text() if trace.exists() else ''
  record('trace-'+policy,all(name+'(' in text for name in ['args_sizes_get','args_get','environ_sizes_get','environ_get']),trace=text)

def dap_round(policy):
 private=Path('/tmp/uwvm-wasif-'+str(time.time_ns()));private.mkdir(mode=0o700)
 server=None;broker=None
 try:
  logfile=(TD/('dap-server-'+policy+'.log')).open('wb')
  argv=[sys.executable,S/'tools/debug/secure_server.py','serve','--uwvm',PRODUCT,'--socket-dir',private,'--','-Rdbg','-Rct','0','-Rllvm-call-stack',policy,'-Rllvm-cache-path','disable','-WFE-gc','-WFE-function-references','--wasip1-global-noinherit-system-environment','--wasip1-global-force-args','2','argv0','before','--wasip1-global-add-or-replace-environment','UWVM_DEBUG_KEY','old','--run',TD/'cli.wasm']
  server=subprocess.Popen([sys.executable,'-c','import os,sys,time;time.sleep(.25);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)',*map(str,argv)],stdout=logfile,stderr=subprocess.STDOUT)
  deadline=time.monotonic()+30
  while not (private/'control.sock').exists():
   assert server.poll() is None and time.monotonic()<deadline,'live server startup';time.sleep(.05)
  spec=importlib.util.spec_from_file_location('live_wasi_dap',S/'tools/debug/dap_adapter.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  broker=module.UnixBroker(str(private));output=io.BytesIO();adapter=module.Adapter(output);adapter.broker=broker
  requests=[]
  def request(command,**args):
   output.seek(0);output.truncate();adapter.handle({'type':'request','seq':1,'command':command,'arguments':args})
   stream=io.BytesIO(output.getvalue());response=[]
   while line:=stream.readline():
    assert line.startswith(b'Content-Length: ') and stream.readline()==b'\r\n'
    v=json.loads(stream.read(int(line[16:-2])))
    if v['type']=='response':response.append(v)
   assert len(response)==1
   requests.append({'command':command,'arguments':args,'response':response[0]})
   (TD/('dap-requests-'+policy+'.json')).write_text(json.dumps(requests,indent=2)+'\n')
   return response[0]
  assert not request('uwvm/wasip1State',selection='env')['success']
  baseline=broker.request('info breakpoints')
  for name,point_policy in [('malformed-condition',{'condition':'false\ncontinue'}),('logpoint',{'logMessage':'guest'}),('exact-hit',{'hitCondition':'4'})]:
   response=request('setInstructionBreakpoints',breakpoints=[{'instructionReference':'wasm:0:4:7',**point_policy}])
   point=response['body']['breakpoints'][0]
   record('live-dap-reject-'+name+'-'+policy,not point['verified'] and broker.request('info breakpoints')==baseline,response=response)
  assert request('evaluate',context='repl',expression='break 0 4 7')['success']
  assert request('evaluate',context='repl',expression='break 0 4 50')['success']
  assert request('evaluate',context='repl',expression='continue')['success']
  deadline=time.monotonic()+30
  while True:
   adapter.observe(broker.request('status'),notify=False)
   if adapter.state=='stopped':break
   assert time.monotonic()<deadline,'live DAP stop';time.sleep(.05)
  for selection in ['args','env','fds','preopens']:
   reply=request('uwvm/wasip1State',selection=selection,moduleId=0)
   assert reply['success'] and reply['body']['available'],reply
  reply=request('uwvm/wasip1Edit',operation='replaceArgument',index=1,value='z'*4096)
  assert reply['success'] and reply['body']['applied'] and reply['body']['stopCurrent'],reply
  read=request('uwvm/wasip1State',selection='args',start=1,count=1)
  assert read['success'] and 'z'*4096 in read['body']['variables'][0]['value'],read
  for operation,args in [('replaceArgument',{'index':1,'value':'after'}),('insertArgument',{'index':1,'value':'middle'}),('removeArgument',{'index':1}),('setEnvironment',{'name':'UWVM_DEBUG_KEY','value':'new'})]:
   reply=request('uwvm/wasip1Edit',operation=operation,**args);assert reply['success'] and reply['body']['applied'],reply
  assert request('evaluate',context='repl',expression='wasm-script set wasip1 arg 0 1 6166746572')['success']
  assert adapter.stop_key is None
  baseline=request('uwvm/wasip1Edit',operation='saveCheckpoint',slot=0)
  assert baseline['success'] and baseline['body']['wasmCheckpointRequired'],baseline
  created=request('uwvm/wasip1Edit',operation='createFile',valueHex='410042')
  assert created['success'] and created['body']['applied'],created
  number=created['body']['descriptor']
  copied=request('uwvm/wasip1Edit',operation='duplicateDescriptor',descriptor=number,expectedBase=0x60006e,expectedInheriting=0)
  assert copied['success'] and copied['body']['descriptor']!=number,copied
  saved=request('uwvm/wasip1Edit',operation='saveCheckpoint',slot=1)
  assert saved['success'] and saved['body']['managedResources']==1 and not saved['body']['externalIORollback'],saved
  strict=request('uwvm/wasip1Edit',operation='restoreCheckpoint',slot=1,resourceMode='strict')
  assert strict['success'] and not strict['body']['applied'] and strict['body']['status']=='resource rollback unavailable',strict
  for n in [number,copied['body']['descriptor']]:
   closed=request('uwvm/wasip1Edit',operation='closeDescriptor',descriptor=n,expectedBase=0x60006e,expectedInheriting=0)
   assert closed['success'] and closed['body']['applied'],closed
  restored=request('uwvm/wasip1Edit',operation='restoreCheckpoint',slot=1,resourceMode='bindings')
  assert restored['success'] and restored['body']['applied'],restored
  restored=request('uwvm/wasip1Edit',operation='restoreCheckpoint',slot=0,resourceMode='bindings')
  assert restored['success'] and restored['body']['applied'],restored
  for slot in [0,1]:
   dropped=request('uwvm/wasip1Edit',operation='dropCheckpoint',slot=slot)
   assert dropped['success'] and dropped['body']['applied'],dropped
  record('live-dap-checkpoint-'+policy,True)
  cp=TD/('dap-portable-'+policy+'.uwp')
  exported=request('uwvm/wasip1Edit',operation='exportPortableCheckpoint',path=str(cp))
  assert exported['success'] and exported['body']['applied'] and exported['body']['wasmCheckpointRequired'],exported
  imported=request('uwvm/wasip1Edit',operation='importPortableCheckpoint',path=str(cp))
  assert imported['success'] and imported['body']['applied'] and imported['body']['contentMode']=='external',imported
  record('live-dap-portable-'+policy,True,response=imported)

  assert request('evaluate',context='repl',expression='trace wasip1 on args_get')['success']
  assert request('evaluate',context='repl',expression='delete 1')['success']
  assert request('evaluate',context='repl',expression='continue')['success']
  deadline=time.monotonic()+30
  while True:
   status=broker.request('status')
   if 'stopped:' in status:break
   assert time.monotonic()<deadline;time.sleep(.05)
  trace=broker.request('trace wasip1 read 0 64')
  assert 'name=args_get' in trace and 'name=args_sizes_get' not in trace and 'phase=return' in trace and 'errno=0 errno-name=esuccess' in trace,trace
  assert request('evaluate',context='repl',expression='delete 2')['success']
  assert request('evaluate',context='repl',expression='continue')['success']
  server.wait(timeout=30)
  assert server.returncode==0,server.returncode
  record('live-dap-'+policy,True,status=status,trace=trace,server_exit=server.returncode)
 except BaseException as e:record('live-dap-'+policy,False,error=repr(e),trace=traceback.format_exc())
 finally:
  if broker is not None:
   try:broker.request('quit')
   except (OSError,ValueError):pass
   broker.close()
  if server is not None:
   shutdown=wait_for_normal_exit(server,10)
   record('live-dap-'+policy+'-managed-close',shutdown.pop('passed'),**shutdown)
   logfile.close()
  # Only this task's private transport directory.
  for file in private.iterdir():file.unlink()
  private.rmdir()
for policy in ['instruction','unwind']:dap_round(policy)
assert run('errno-wat-parse',[W,'parse',S/'test/0017.runtime/fixtures/debug_wasip1_calls.wat','-o',TD/'errno.wasm'])
assert run('errno-wat-validate',[W,'validate',TD/'errno.wasm','--features','all'])
for policy in ['instruction','unwind']:
 c=None
 try:
  c=Console(policy,name='errno',fixture='errno.wasm')
  checked(c,'enable-before-entry','trace wasip1 on fd_close',lambda x:'enabled=1 filter=fd_close' in x)
  c.send('continue');ended=c.until('guest exited:')
  record(c.name+'-actual-ebadf-result','guest exited: 0' in ended,reply=ended)
  checked(c,'actual-ebadf-pair','trace wasip1 read 0 64',lambda x:x.count('phase=entry')==1 and x.count('phase=return')==1 and 'arg0=i32:0xffffffff' in x and 'errno=8 errno-name=ebadf' in x)
  checked(c,'off','trace wasip1 off',lambda x:'enabled=0' in x)
  checked(c,'clear','trace wasip1 clear',lambda x:'phase=' not in x and 'remaining=0' in x)
 except Exception as error:record('errno-'+policy+'-exception',False,error=repr(error),trace=traceback.format_exc())
 finally:
  if c is not None:c.close()
# FD0 is an intentional null resource, not a free-list cell. Restore must
# preserve original guest fd_close behavior without exposing console input.
(TD/'reserved.wat').write_text('(module (type $node (struct (field i32)))\n(import "wasi_snapshot_preview1" "fd_close" (func $close (param i32) (result i32)))\n(func $run (export "run") (result i32) (local $n (ref $node))\n i32.const 7 struct.new $node local.set $n nop\n i32.const 0 call $close if unreachable end\n local.get $n struct.get $node 0 drop i32.const 91)\n(func (export "_start") call $run i32.const 91 i32.ne if unreachable end))\n')
assert run('reserved-wat-parse',[W,'parse',TD/'reserved.wat','-o',TD/'reserved.wasm'])
assert run('reserved-wat-validate',[W,'validate',TD/'reserved.wasm','--features','all'])
for policy in ['instruction','unwind']:
 c=None
 try:
  c=Console(policy,name='reserved',fixture='reserved.wasm')
  c.send('break 0 1 7');c.send('continue');c.until('stopped: breakpoint')
  checked(c,'save','set wasip1 checkpoint 0 0',lambda x:ok(x) and 'retained-external=2' in x)
  checked(c,'construct','set wasip1 file 0 -',lambda x:ok(x) and 'affected=3' in x)
  checked(c,'restore','set wasip1 restore 0 0 bindings',ok)
  checked(c,'reserved-not-allocated','set wasip1 file 0 -',lambda x:ok(x) and 'affected=3' in x)
  checked(c,'restore-again','set wasip1 restore 0 0 bindings',ok)
  c.send('unset wasip1 checkpoint 0 0');c.send('trace wasip1 on fd_close');c.send('delete 1');c.send('continue')
  ended=c.until('guest exited:')
  record(c.name+'-original-fd0-close-and-management-input','guest exited: 0' in ended,reply=ended)
  checked(c,'actual-guest-close-null-resource','trace wasip1 read 0 64',lambda x:'arg0=i32:0x0' in x and 'errno=0 errno-name=esuccess' in x)
 except Exception as error:record('reserved-'+policy+'-exception',False,error=repr(error),trace=traceback.format_exc())
 finally:
  if c is not None:c.close()
# Multiple real initialized environments; one batch file and one current stop.
# The tiny main validates original WASI arguments after restoration, while the
# provider receives a separately owned WASIp1 environment from the real loader.
(TD/'group-provider.wat').write_text('(module (import "wasi_snapshot_preview1" "fd_close" (func (param i32) (result i32))) (memory (export "memory") 1) (func (export "ping") nop))')
assert run('group-provider-parse',[W,'parse',TD/'group-provider.wat','-o',TD/'group-provider.wasm'])
assert run('group-provider-validate',[W,'validate',TD/'group-provider.wasm','--features','all'])
group_extra=['--wasm-preload-library',TD/'group-provider.wasm','wasi-group-provider',
 '--wasip1-single-create','wasi-group-provider',
 '--wasip1-single-noinherit-system-environment','wasi-group-provider',
 '--wasip1-single-force-args','wasi-group-provider','2','provider0','provider-before',
 '--wasip1-single-add-or-replace-environment','wasi-group-provider','UWVM_DEBUG_KEY','provider-before']
def group_start(send,wait):
 accepted=[]
 for mid in (0,1):
  response=send(f'break {mid} 4 7')
  if 'error:' not in response:accepted.append(mid)
 assert len(accepted)==1,accepted
 send('continue');wait('stopped: breakpoint')
 main=accepted[0];provider=1-main
 assert '"argv0"' in send(f'info wasip1 args {main}')
 assert '"provider0"' in send(f'info wasip1 args {provider}')
 return main,provider

def nested_snapshots(path):
 data=path.read_bytes();assert data[:8]==b'UWWSGP1\0' and int.from_bytes(data[16:20],'little')==2
 pos=20;chunks=[]
 for _ in range(2):
  size=int.from_bytes(data[pos:pos+4],'little');chunks.append((pos+4,size));pos+=4+size
 assert pos==len(data)-32
 return data,chunks

def anonymous_resource_indices(path):
 data,chunks=nested_snapshots(path);indices=[]
 for start,size in chunks:
  raw=data[start:start+size];counts=[int.from_bytes(raw[100+4*i:104+4*i],'little') for i in range(6)];pos=124
  for _ in range(counts[0]+counts[1]):length=int.from_bytes(raw[pos:pos+4],'little');pos+=4+length
  external=[]
  for i in range(counts[2]):
   if raw[pos]==3:external.append(i)
   pos+=16
   for _ in range(2):length=int.from_bytes(raw[pos:pos+4],'little');pos+=4+length
  assert len(external)==1,external;indices.append(external[0])
 return indices

for policy in ['instruction','unwind']:
 c=None
 try:
  c=Console(policy,group_extra,name='group')
  main,provider=group_start(c.send,c.until)
  path=TD/('cli-group-'+policy+'.uwpg');hx=str(path).encode().hex()
  stop=int(re.search(r'stop-id (\d+)',c.send('status'))[1])
  fds=[]
  for mid in (main,provider):
   made=checked(c,'create-'+str(mid),f'set wasip1 file {mid} 410042',ok);fd=int(re.search(r'affected=(\d+)',made)[1]);fds.append(fd)
   checked(c,'alias-'+str(mid),f'set wasip1 fd-dup {mid} {fd} 0x60006e 0',ok)
  checked(c,'export',f'set wasip1 export-group {hx} {main} {provider} if-stop {stop}',lambda x:ok(x) and 'environments=2' in x and 'atomic=true' in x and 'checkpoint Wasm and WASIp1 together' in x)
  digest=hashlib.sha256(path.read_bytes()).hexdigest();indices=anonymous_resource_indices(path)
  checked(c,'exclusive',f'set wasip1 export-group {hx} {main} {provider}',lambda x:'applied=0' in x and 'native resource operation failed' in x)
  record(c.name+'-exclusive-preserved',hashlib.sha256(path.read_bytes()).hexdigest()==digest)
  for mid in (main,provider):checked(c,'change-'+str(mid),f'set wasip1 arg {mid} 1 77726f6e67',ok)
  checked(c,'second-missing-binding',f'set wasip1 import-group {hx} {main}:{indices[0]}={fds[0]} {provider}',lambda x:'request=1' in x and 'applied=0' in x)
  for mid in (main,provider):checked(c,'failed-group-intact-'+str(mid),f'info wasip1 args {mid}',lambda x:'"wrong"' in x)
  bad=TD/('cli-group-wrong-source-'+policy+'.uwpg');data,chunks=nested_snapshots(path);data=bytearray(data);start,size=chunks[1];data[start+32]^=1
  data[start+size-32:start+size]=hashlib.sha256(data[start:start+size-32]).digest();data[-32:]=hashlib.sha256(data[:-32]).digest();bad.write_bytes(data)
  checked(c,'later-wrong-source',f'set wasip1 import-group {str(bad).encode().hex()} {main}:{indices[0]}={fds[0]} {provider}:{indices[1]}={fds[1]}',lambda x:'request=1' in x and 'applied=0' in x and 'stale stop or generation' in x)
  for mid in (main,provider):checked(c,'source-refusal-intact-'+str(mid),f'info wasip1 args {mid}',lambda x:'"wrong"' in x)
  checked(c,'stale-stop',f'set wasip1 import-group {hx} {main}:{indices[0]}={fds[0]} {provider}:{indices[1]}={fds[1]} if-stop {stop+1}',lambda x:'applied=0' in x and 'stale stop or generation' in x)
  checked(c,'import',f'set wasip1 import-group {hx} {main}:{indices[0]}={fds[0]} {provider}:{indices[1]}={fds[1]} if-stop {stop}',lambda x:ok(x) and 'applied=1' in x and 'environments=2' in x)
  for mid,value in ((main,'before'),(provider,'provider-before')):
   checked(c,'restored-'+str(mid),f'info wasip1 args {mid}',lambda x,value=value:'"'+value+'"' in x and '"wrong"' not in x)
   rows_text=checked(c,'restored-alias-'+str(mid),f'info wasip1 fds {mid}',ok);bindings=re.findall(r'managed-resource=(\d+)',rows_text)
   record(c.name+'-alias-topology-'+str(mid),len(bindings)>=4 and bindings[-1]==bindings[-2] and bindings[-1]!='0')
  checked(c,'duplicate-modules',f'set wasip1 export-group {hx} {main} {main}',lambda x:'error:' in x)
  checked(c,'too-many-environments',f'set wasip1 export-group {hx} '+' '.join(map(str,range(17))),lambda x:'error:' in x)
 except Exception as error:record('group-'+policy+'-exception',False,error=repr(error),trace=traceback.format_exc())
 finally:
  if c is not None:c.close()

def dap_group_round(policy):
 private=Path('/tmp/uwvm-wasig-'+str(time.time_ns()));private.mkdir(mode=0o700);server=None;broker=None;transport=[]
 try:
  logfile=(TD/('dap-group-server-'+policy+'.log')).open('wb')
  argv=[sys.executable,S/'tools/debug/secure_server.py','serve','--uwvm',PRODUCT,'--socket-dir',private,'--',
   '-Rdbg','-Rct','0','-Rllvm-call-stack',policy,'-Rllvm-cache-path','disable','-WFE-gc','-WFE-function-references',
   '--wasip1-global-noinherit-system-environment','--wasip1-global-force-args','2','argv0','before',
   '--wasip1-global-add-or-replace-environment','UWVM_DEBUG_KEY','old',*group_extra,'--run',TD/'cli.wasm']
  server=subprocess.Popen([sys.executable,'-c','import os,sys,time;time.sleep(.25);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)',*map(str,argv)],stdout=logfile,stderr=subprocess.STDOUT)
  end=time.monotonic()+30
  while not (private/'control.sock').exists():assert server.poll() is None and time.monotonic()<end;time.sleep(.05)
  spec=importlib.util.spec_from_file_location('group_live_dap',S/'tools/debug/dap_adapter.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  broker=module.UnixBroker(str(private));original_request=broker.request
  def authenticated(command):
   response=original_request(command);transport.append({'command':command,'reply':response});return response
  broker.request=authenticated
  def wait(marker):
   end=time.monotonic()+30
   while marker not in broker.request('status'):assert time.monotonic()<end;time.sleep(.05)
  main,provider=group_start(broker.request,wait)
  output=io.BytesIO();adapter=module.Adapter(output);adapter.broker=broker
  def request(**args):
   output.seek(0);output.truncate();adapter.handle({'type':'request','seq':1,'command':'uwvm/wasip1Edit','arguments':args})
   stream=io.BytesIO(output.getvalue());replies=[]
   while line:=stream.readline():
    assert line.startswith(b'Content-Length: ') and stream.readline()==b'\r\n';item=json.loads(stream.read(int(line[16:-2])))
    if item['type']=='response':replies.append(item)
   assert len(replies)==1;return replies[0]
  fds=[]
  for mid in (main,provider):
   reply=request(operation='createFile',moduleId=mid,valueHex='410042');assert reply['success'] and reply['body']['applied'],reply;fds.append(reply['body']['descriptor'])
  path=TD/('dap-group-'+policy+'.uwpg');environments=[{'moduleId':mid} for mid in (main,provider)]
  reply=request(operation='exportPortableCheckpointGroup',path=str(path),environments=environments)
  record('live-dap-group-export-'+policy,reply['success'] and reply['body']['applied'] and reply['body']['environmentCount']==2 and reply['body']['groupAtomic'] and reply['body']['wasmCheckpointRequired'],response=reply)
  indices=anonymous_resource_indices(path)
  for mid in (main,provider):
   changed=request(operation='replaceArgument',moduleId=mid,index=1,value='changed');assert changed['success'] and changed['body']['applied'],changed
  targets=[{'moduleId':mid,'bindings':[{'resource':index,'descriptor':fd}]} for mid,index,fd in zip((main,provider),indices,fds)]
  bad=[targets[0],{'moduleId':provider}];reply=request(operation='importPortableCheckpointGroup',path=str(path),environments=bad)
  record('live-dap-group-later-refusal-'+policy,reply['success'] and not reply['body']['applied'] and 'request=1' in reply['body']['diagnostic'],response=reply)
  for mid in (main,provider):record('live-dap-group-no-partial-'+str(mid)+'-'+policy,'"changed"' in broker.request(f'info wasip1 args {mid}'))
  count=len(transport);invalid=request(operation='exportPortableCheckpointGroup',path=str(path),environments=[{'moduleId':main},{'moduleId':main}])
  record('live-dap-group-invalid-before-transport-'+policy,not invalid['success'] and len(transport)==count,response=invalid)
  reply=request(operation='importPortableCheckpointGroup',path=str(path),environments=targets)
  record('live-dap-group-restore-'+policy,reply['success'] and reply['body']['applied'] and reply['body']['environmentCount']==2,response=reply)
  for mid,value in ((main,'before'),(provider,'provider-before')):record('live-dap-group-restored-'+str(mid)+'-'+policy,'"'+value+'"' in broker.request(f'info wasip1 args {mid}'))
 except Exception as error:record('dap-group-'+policy+'-exception',False,error=repr(error),trace=traceback.format_exc())
 finally:
  (TD/('dap-group-transport-'+policy+'.json')).write_text(json.dumps(transport,indent=2)+'\n')
  if broker is not None:
   try:broker.request('quit')
   except Exception:pass
   broker.close()
  if server is not None:
   shutdown=wait_for_normal_exit(server,15)
   record('dap-group-'+policy+'-managed-close',shutdown.pop('passed'),**shutdown)
   logfile.close()
  import shutil
  shutil.rmtree(private)
for policy in ['instruction','unwind']:dap_group_round(policy)

raise SystemExit(0 if all(row['passed'] for row in rows) else 1)

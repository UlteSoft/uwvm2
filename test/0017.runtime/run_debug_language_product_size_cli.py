#!/usr/bin/env python3
"""Finite current full-product regression; not full language/native parity.

Requires the fresh CLI/runtime/host receipt produced by the paired Linux build.
The compiler writes original oracle locals; no Wasm/DWARF bytes are rewritten.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shlex,subprocess,sys,time
sys.dont_write_bytecode=True
ap=argparse.ArgumentParser(description="Compare live -Rdbg expressions with original target compiler values in the controlled Linux cgroup")
ap.add_argument('--source-root',type=Path,required=True)
ap.add_argument('--uwvm',type=Path,required=True)
ap.add_argument('--build-receipt',type=Path,required=True)
ap.add_argument('--clang',type=Path,required=True)
ap.add_argument('--wasm-ld',type=Path,required=True)
ap.add_argument('--wasm-tools',type=Path,required=True)
ap.add_argument('--llvm-dwarfdump',type=Path,required=True)
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--ros',action='store_true')
a=ap.parse_args()
assert sys.platform=='linux','Execute only in the Linux test cgroup'
S=a.source_root.resolve(strict=True);O=a.out.resolve();O.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
sys.path.insert(0,str(S/'test/0017.runtime'));from run_debug_source_inline_metadata_cli import Console
compiler=a.clang.resolve(strict=True);validator=a.wasm_tools.resolve(strict=True)
dwarfdump=a.llvm_dwarfdump.resolve(strict=True)
SDK=a.wasm_ld.absolute().parent.parent
fixture=S/'test/0017.runtime/fixtures/debug_language_product_size.c';binary=a.uwvm.resolve(strict=True);receipt=a.build_receipt.resolve(strict=True)
build=json.loads(receipt.read_text());assert build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and sha(binary)==build['binary_sha256']
repository='uwvm2-ros' if a.ros else 'uwvm2'
for p,h in build['inputs_before'].items():
 if Path(p).is_relative_to(S):assert sha(p)==h,('Build source changed',p)
pins={str(p.resolve()):sha(p) for p in [Path(__file__),S/'test/0017.runtime/run_debug_source_inline_metadata_cli.py',compiler,validator,dwarfdump,a.wasm_ld,fixture,binary,receipt]}
record={'passed':False,'repository':repository,'build_source_id':build['source_id'],'product_sha256':sha(binary),'build_receipt_sha256':sha(receipt),'inputs':pins,'producer_rows':[],'cases':[],'cgroup':Path('/proc/self/cgroup').read_text(),'source_optimization':0,'link_optimization':0,'scope':'Actual -Rdbg LLVM-full language expressions against compiler-produced volatile live oracle values; fresh paired product'}
def save():(O/'results.json').write_text(json.dumps(record,indent=2)+'\n')
def run(argv,label):
 log=O/(label+'.log');start=time.monotonic()
 with log.open('wb') as f:r=subprocess.run(argv,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 row={'argv':argv,'label':label,'returncode':r.returncode,'seconds':time.monotonic()-start,'log_sha256':sha(log)};record['producer_rows'].append(row);save();assert r.returncode==0,log.read_text(errors='replace')[-4000:]
expressions=['value + 2','sizeof(value)','sizeof(packet)','sizeof(sizeof(value))','sizeof(sizeof(value)+wide)','sizeof(sizeof(value)+wider)','sizeof(1?sizeof(value):wide)','sizeof(1?small:small)','sizeof(1&&value)',"sizeof('a')",'sizeof(flag)','sizeof(+small)','(unsigned long)1+sizeof(value)','1?sizeof(value):wide','small+byte','sizeof(0?sizeof(packet):sizeof(value))','value==7&&flag','value<<1']
line=next(i for i,s in enumerate(fixture.read_text().splitlines(),1) if 'LANGUAGE_PRODUCT_READY' in s)
def numeric(text):
 assert 'error:' not in text and 'rejected' not in text and 'unavailable' not in text,text
 values=re.findall(r'(?m)^(?!source-value\b)[^\r\n]*\b(?:value=|[ui](?:8|16|32|64)=)(true|false|-?\d+)\b',text)
 assert len(values)==1,text
 return 1 if values[0]=='true' else 0 if values[0]=='false' else int(values[0])
subprocess.run(['bash',str(S/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);save()
run([str(binary),'--version'],'product-version')
run([str(binary),'--help'],'product-help')
run([str(binary),'--help','runtime'],'product-help-runtime')
assert '--runtime-debug' in (O/'product-help-runtime.log').read_text(errors='replace')
for bits in [32,64]:
 for lang,dialect in [('c','c17'),('c23','c23'),('cpp','c++20'),('objc','c17'),('objcpp','c++20')]:
  for dwarf in [4,5]:
   stem=f'{lang}-wasm{bits}-dwarf{dwarf}';wasm=O/(stem+'.wasm');x={'c':'c','c23':'c','cpp':'c++','objc':'objective-c','objcpp':'objective-c++'}[lang]
   # LLD -O0 retains separate debug strings. The original default-link DWARF5
   # fixture and LLVM suffix-string verifier rejection remain in evidence.
   run([str(compiler),'--target=wasm'+str(bits)+'-unknown-unknown','-fuse-ld=lld','-B'+str(SDK/'bin'),'-x',x,'-std='+dialect,'-O0','-g','-gdwarf-'+str(dwarf),'-nostdlib','-Wl,-O0','-Wl,--no-entry','-Wl,--export-all','-Wl,--initial-memory=131072',str(fixture),'-o',str(wasm)],stem+'-compile')
   run([str(validator),'validate','--features','all',str(wasm)],stem+'-validate')
   run([str(dwarfdump),'--verify',str(wasm)],stem+'-dwarf-verify');pins[str(wasm)]=sha(wasm)
   for policy in ['instruction','unwind']:
    row={'stem':stem,'policy':policy,'wasm_sha256':sha(wasm),'passed':False,'actions':[],'checks':[]};record['cases'].append(row);save();console=None
    try:
     prefix=[str(binary),'-Rdbg']
     if not a.ros:prefix+=['-Rcc','jit','-Rcm','full']
     prefix+=['-Rct','0','-Rllvm-call-stack',policy,'-Rllvm-cache-path','disable']
     if bits==64:prefix+=['--wasm-feature-enable-memory64','--wasm-feature-enable-table64']
     prefix+=['--run',str(wasm)];row['argv']=prefix
     console=Console(prefix,O/(stem+'-'+policy+'-console.log'))
     def ask(command):
      text=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',console.send(command)).decode(errors='replace');row['actions'].append({'command':command,'reply':text});save();return text
     br=ask(f'break-source 0 {fixture}:{line}');assert 'error:' not in br and 'breakpoint' in br,br
     ask('continue')
     for attempt in range(64):
      stop=ask('wait')
      if 'stopped: breakpoint' in stop:break
      assert 'guest exited:' not in stop,stop
     else:raise AssertionError('No source breakpoint stop')
     thread=int(re.search(r'thread (\d+) module=0 function=',stop)[1]);bt=ask(f'bt {thread}');sid=int(re.search(r'(?m)^stop-id (\d+)',bt)[1])
     assert numeric(ask(f'print {thread} {sid} value'))==7
     for i,expr in enumerate(expressions):
      oracle=numeric(ask(f'print {thread} {sid} oracle{i}'));reply=ask(f'print {thread} {sid} {expr}');actual=numeric(reply)
      row['checks'].append({'expression':expr,'target_compiler_oracle':oracle,'debugger_value':actual,'passed':actual==oracle});save();assert actual==oracle,(expr,actual,oracle,reply)
     for expr in ['sizeof(missing)','value++','value=99']:
      reply=ask(f'print {thread} {sid} {expr}');assert 'error:' in reply or 'rejected' in reply or 'unavailable' in reply,(expr,reply)
     assert numeric(ask(f'print {thread} {sid} value'))==7
     reply=ask(f'print-frame {thread} {sid} 0 sizeof(sizeof(value)+wider)');assert numeric(reply)==8
     assert 'error:' in ask(f'print {thread} {sid+1000000} value')
     ask('delete '+re.search(r'breakpoint (\d+)',br)[1])
     stepped=ask(f'step wasm {thread}');assert 'guest exited:' not in stepped,stepped
     stale=ask(f'print {thread} {sid} value');assert 'error:' in stale and 'source-value stop=' not in stale,stale
     ask('continue')
     for attempt in range(64):
      exited=ask('wait')
      if 'guest exited:' in exited:
       assert 'guest exited: 0' in exited,exited
       break
     else:raise AssertionError('Guest completion deadline')
     row['real_retired_stop_rejected']=True;row['guest_completed_without_trap']=True
     row['passed']=True;save();print(repository,stem,policy,'PASS',len(row['checks']),'live compiler comparisons',flush=True)
    except BaseException as error:
     row['error']=repr(error);save();raise
    finally:
     if console:
      try:
       console.finish();row['quit_returncode']=console.child.returncode;row['managed_shutdown_complete']='managed shutdown complete' in bytes(console.transcript).decode(errors='replace')
       assert row['quit_returncode']==0 and row['managed_shutdown_complete'],'Debugger shutdown failed'
      except BaseException as error:row.update(passed=False,shutdown_error=repr(error));raise
      finally:save()
for p,h in pins.items():assert sha(p)==h,('Input changed',p)
assert len(record['cases'])==40 and all(r['passed'] and len(r['checks'])==18 and r['quit_returncode']==0 and r['managed_shutdown_complete'] for r in record['cases'])
record.update(passed=True,all_inputs_after_unchanged=True,live_compiler_comparisons=sum(len(r['checks']) for r in record['cases']));save();print(repository,'PASS LIVE PRODUCT',len(record['cases']),record['live_compiler_comparisons'],flush=True)

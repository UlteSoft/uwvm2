#!/usr/bin/env python3
"""Real own-file source breakpoints for the five DWARF producer routes."""
from __future__ import annotations
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
import run_debug_source_step_cli as step
import run_debug_source_tinygo_cli as tinygo
import run_debug_language_experience_cli as language

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('uwvm','wasm','source','oracle','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--language',choices=('c','cpp','rust','objc','tinygo'),required=True)
 ap.add_argument('--diagnostic-policy',choices=('instruction','unwind'),default='instruction');ap.add_argument('--ros',action='store_true');a=ap.parse_args()
 root=Path(__file__).resolve().parents[2];assert sys.platform=='linux';subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.out.mkdir(parents=True,exist_ok=False)
 paths=[a.uwvm,a.wasm,a.source,a.oracle,Path(__file__),Path(step.__file__),Path(tinygo.__file__),Path(language.__file__)];pins={str(p):language.sha(p) for p in paths}
 d={'passed':False,'language':a.language,'inputs_before':pins,'actions':[],'positions':[]};s=None
 try:
  mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'];argv=[str(a.uwvm),'-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',a.diagnostic_policy,'-Rllvm-cache-path','disable','--run',str(a.wasm)]
  s=step.Session(argv,a.out/'console.log',step.code_expressions(a.wasm),step.line_sequences(a.oracle.read_text()),a.source);d.update(argv=argv,actions=s.actions,positions=s.positions)
  target,marker=(tinygo.named_function(a.wasm,b'_i_LanguageProbe__add_')[0],'OBJC_METHOD_READY') if a.language=='objc' else (tinygo.named_function(a.wasm,b'main.probeOuter')[0],'VALUES') if a.language=='tinygo' else (step.metadata_cli.function(a.wasm,'numeric_probe')[0],'NUMERIC_READY')
  matches=[i for i,line in enumerate(a.source.read_text().splitlines(),1) if '/* '+marker+' */' in line or '// '+marker in line];assert len(matches)==1;wanted=matches[0]
  s.begin(target);origin=language.own_seek(s,lambda p:p['function']==target and p['is_statement'] and p['line']<wanted,'mapped source statement before later breakpoint')
  command=f'break-source 0 {origin["file"]}:{wanted}';reply=language.query(s,command);found=re.search(rb'breakpoint (\d+)',reply);assert found is not None,(command,reply);bid=found[1].decode()
  prior=language.query(s,'info breakpoints');bad=language.query(s,f'break-source 0 {origin["file"]}.not-a-producer-file:{wanted}');assert b'error:' in bad and prior==language.query(s,'info breakpoints'),bad
  s.send('continue')
  for _ in range(20):
   reply=s.send('wait')
   if b'stopped: breakpoint' in reply:break
  else:raise AssertionError('file/line breakpoint did not stop real guest')
  position=s.position();assert position['function']==target and position['line']==wanted and position['is_statement'],position
  expected=7 if a.language=='objc' else 1.25;expression='self->value' if a.language=='objc' else 'decimal' if a.language=='tinygo' else 'decimal32';value=language.command_value(s,position,expression)
  numbers=re.findall(rb'(?:, value=| value=| = f(?:32|64)=| = [iu]\d+=)([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)(?: \([^\r\n]*\))?\r?$',value,re.M);assert len(numbers)==1 and float(numbers[0])==expected,value
  d.update(actual_source_break_command=command,expected_value=expected,actual_value_reply=value.decode(),actual_break_stop=position);s.send('delete '+bid);s.finish_guest();d['passed']=True
 except BaseException as e:d['error']=repr(e);raise
 finally:
  if s is not None:
   try:s.console.finish()
   except BaseException as e:d.update(passed=False,close_error=repr(e));raise
   finally:d.update(quit_returncode=s.console.child.returncode,managed_shutdown_complete=b'managed shutdown complete' in s.console.transcript)
  d['inputs_after']={p:language.sha(Path(p)) for p in pins}
  if d['inputs_after']!=pins:d.update(passed=False,inputs_changed=True)
  (a.out/'summary.json').write_text(json.dumps(d,indent=2)+'\n')
 assert d['passed'];print('PASS real '+a.language+' file/line break and unchanged table after invalid path')

if __name__=='__main__':main()

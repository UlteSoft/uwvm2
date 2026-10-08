#!/usr/bin/env python3
"""Genuine C++ member values and type names; finite native name coverage."""
from __future__ import annotations
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
import run_debug_source_step_cli as step
import run_debug_source_tinygo_cli as tinygo
import run_debug_language_experience_cli as language

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('uwvm','wasm','source','oracle','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--const-method',action='store_true')
 ap.add_argument('--language',choices=('cpp',),required=True)
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
  # This producer uses a real C++ member method, and real copied `this`.
  for expression,expected in [('this->value',7),('amount',4),('cookie',11)]:
   actual=language.command_value(s,position,expression);numbers=re.findall(rb'(?:, value=| value=| = [iu]\d+=)([-+]?\d+)\r?$',actual,re.M);assert len(numbers)==1 and int(numbers[0])==expected,(expression,actual)
   d.setdefault('actual_method_values',[]).append({'expression':expression,'expected':expected,'reply':actual.decode()})
  expected_type='const LanguageThis *' if a.const_method else 'LanguageThis *'
  names=[('this',expected_type)]
  if a.const_method:
   names += [('saved_this','const LanguageThis * const'),('reference','const LanguageThis &'),
             ('rvalue_reference','const LanguageThis &&'),('pointer_to_saved','const LanguageThis * const *'),('alias_this','ConstProbe *')]
   for expression in ['saved_this->value','reference.value','rvalue_reference.value','alias_this->value']:
    actual=language.command_value(s,position,expression);numbers=re.findall(rb'(?:, value=| value=| = [iu]\d+=)([-+]?\d+)\r?$',actual,re.M)
    assert len(numbers)==1 and int(numbers[0])==7,(expression,actual)
    d.setdefault('actual_qualified_alias_values',[]).append({'expression':expression,'expected':7,'reply':actual.decode()})
  for expression,expected_type in names:
   metadata=language.query(s,f'ptype {s.thread} {position["stop_id"]} {expression}')
   assert b'source-type stop=' in metadata and b'kind=pointer' in metadata and b'byte-size=4' in metadata,metadata
   assert b' type='+expected_type.encode()+b' kind=pointer' in metadata,(expression,expected_type,metadata)
   d.setdefault('actual_pointer_type_names',[]).append({'expression':expression,'expected':expected_type,'reply':metadata.decode()})
  d['native_pointer_type_name_qualified']=True
  d['native_this_type_complete']=False # precise tested subset, not every native declarator/runtime feature
  d['remaining_native_gap']='function/member-pointer/anonymous-array declarators and complete language-specific type spelling remain outside this finite qualification'
  layout=language.query(s,f'ptype {s.thread} {position["stop_id"]} *this');assert b'source-type stop=' in layout and b'LanguageThis' in layout and b'name=value' in layout,layout;d['actual_this_object_layout_reply']=layout.decode()
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

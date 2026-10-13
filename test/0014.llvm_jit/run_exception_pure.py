#!/usr/bin/env python3
"""Compare production pure validator with Wasmtime for current try_table/catch/throw grammar.
The empty _start keeps valid uncaught throws out of execution; every subject body is still validated.
"""
import argparse,json,subprocess,resource
from pathlib import Path
TAGS='(tag $empty) (tag $int (param i32)) (tag $pair (param i32 i64))'
CASES=[
 ('imported-tag-throw',True,'(result i32)','i32.const 1 throw $import'),
 ('imported-tag-catch',True,'(result i32)','try_table (catch $import 0) i32.const 1 throw $import end unreachable'),
 ('imported-shifts-local-tag',True,'','try_table (catch $empty 0) throw $empty end'),
 ('imported-tag-wrong-payload',False,'','i64.const 1 throw $import'),
 ('catch-all-outer-void',True,'','try_table (result i32) (catch_all 0) i32.const 1 end drop'),
 ('catch-all-wrong-outer',False,'(result i32)','try_table (catch_all 0) end i32.const 1'),
 ('catch-own-label-is-not-visible',False,'','try_table (catch_all 1) end'),
 ('catch-outer-tag-payload',True,'(result i32)','try_table (catch $int 0) i32.const 42 throw $int end unreachable'),
 ('catch-pair',True,'(result i32 i64)','try_table (catch $pair 0) i32.const 42 i64.const 7 throw $pair end unreachable'),
 ('catch-pair-wrong-order',False,'(result i64 i32)','try_table (catch $pair 0) unreachable end unreachable'),
 ('catch-empty',True,'','try_table (catch $empty 0) throw $empty end'),
 ('catch-empty-wrong-arity',False,'(result i32)','try_table (catch $empty 0) unreachable end unreachable'),
 ('catch-all-wrong-arity',False,'(result i32)','try_table (catch_all 0) unreachable end unreachable'),
 ('catch-nested-depth',True,'(result i32)','block try_table (catch $int 1) unreachable end end unreachable'),
 ('catch-nested-wrong-depth',False,'(result i32)','block try_table (catch $int 0) unreachable end end unreachable'),
 ('catch-loop-parameters',True,'','i32.const 1 loop (param i32) (result i64) drop try_table (catch $int 0) i32.const 2 throw $int end unreachable end drop'),
 ('catch-loop-not-results',False,'','i32.const 1 loop (param i32) (result i64) drop try_table (catch $pair 0) unreachable end unreachable end drop'),
 ('catch-duplicates',True,'','try_table (catch $empty 0) (catch $empty 0) throw $empty end'),
 ('catch-all-before-tag',True,'','try_table (catch_all 0) (catch $empty 0) throw $empty end'),
 ('unknown-catch-tag',False,'','try_table (catch 99 0) end'),
 ('unknown-catch-label',False,'','try_table (catch $empty 99) end'),
 ('throw-empty',True,'','throw $empty'),
 ('throw-payload',True,'(result f64)','i32.const 2 i64.const 3 throw $pair'),
 ('throw-polymorphic',True,'','unreachable throw $pair'),
 ('throw-polymorphic-wrong-concrete',False,'','unreachable f32.const 2 throw $pair'),
 ('throw-underflow',False,'','throw $int'),
 ('throw-wrong-type',False,'','i64.const 1 throw $int'),
 ('throw-unknown-tag',False,'','unreachable throw 99'),
 ('throw-preserves-outer-base',True,'(result i32)','i32.const 7 try_table throw $empty end'),
 ('throw-ref-polymorphic',True,'','unreachable throw_ref'),
 ('throw-ref-bottom',True,'','unreachable ref.as_non_null throw_ref'),
 ('throw-ref-live-null-exn',True,'','ref.null exn throw_ref'),
 ('throw-ref-live-null-noexn',True,'','ref.null noexn throw_ref'),
 ('throw-ref-dead-concrete-exn',True,'','unreachable ref.null exn throw_ref'),
 ('throw-ref-null-exn-test',True,'','ref.null exn ref.is_null drop'),
 ('throw-ref-null-exn-nonnull',True,'','ref.null exn ref.as_non_null throw_ref'),
 ('throw-ref-underflow',False,'','throw_ref'),
 ('throw-ref-function',False,'','ref.null func throw_ref'),
 ('throw-ref-external',False,'','ref.null extern throw_ref'),
 ('throw-ref-number',False,'','i32.const 0 throw_ref'),
 ('throw-ref-polymorphic-number',False,'','unreachable i32.const 0 throw_ref'),
 ('throw-ref-null-exn-as-number',False,'','ref.null exn i32.const 0 i32.add'),
 ('catch-ref-needs-exn',False,'(result i32 externref)','try_table (catch_ref $int 0) unreachable end unreachable'),
 ('catch-all-ref-needs-exn',False,'(result funcref)','try_table (catch_all_ref 0) unreachable end unreachable'),
 ('catch-ref-tagged-nonnull',True,'(result i32 (ref exn))','try_table (catch_ref $int 0) i32.const 41 throw $int end unreachable'),
 ('catch-ref-tagged-nullable',True,'(result i32 exnref)','try_table (catch_ref $int 0) i32.const 41 throw $int end unreachable'),
 ('catch-ref-tagged-pair',True,'(result i32 i64 (ref exn))','try_table (catch_ref $pair 0) i32.const 41 i64.const 42 throw $pair end unreachable'),
 ('catch-all-ref-nonnull',True,'(result (ref exn))','try_table (catch_all_ref 0) throw $empty end unreachable'),
 ('catch-all-ref-nullable',True,'(result exnref)','try_table (catch_all_ref 0) throw $empty end unreachable'),
 ('catch-ref-wrong-payload-type',False,'(result i64 (ref exn))','try_table (catch_ref $int 0) i32.const 41 throw $int end unreachable'),
 ('catch-ref-wrong-payload-order',False,'(result i64 i32 (ref exn))','try_table (catch_ref $pair 0) i32.const 41 i64.const 42 throw $pair end unreachable'),
 ('catch-all-ref-extra-result',False,'(result i32 (ref exn))','try_table (catch_all_ref 0) throw $empty end unreachable'),
]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--validator',type=Path,required=True);p.add_argument('--wasmtime',type=Path,required=True);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--focus-exnref',action='store_true',help='run only newly added exnref grammar cases');a=p.parse_args();root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False);rows=[]
 provider=a.out/'provider.wasm';wat=a.out/'provider.wat';wat.write_text('(module (tag (export "int") (param i32)) (func (export "nop")))');subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(provider)],check=True)
 focus={'throw-ref-live-null-exn','throw-ref-live-null-noexn','throw-ref-dead-concrete-exn','throw-ref-null-exn-test','throw-ref-null-exn-nonnull','throw-ref-null-exn-as-number','catch-ref-needs-exn','catch-all-ref-needs-exn',
        'catch-ref-tagged-nonnull','catch-ref-tagged-nullable','catch-ref-tagged-pair','catch-all-ref-nonnull','catch-all-ref-nullable',
        'catch-ref-wrong-payload-type','catch-ref-wrong-payload-order','catch-all-ref-extra-result'}
 selected=[case for case in CASES if not a.focus_exnref or case[0] in focus]
 for name,valid,sig,body in selected:
  path=a.out/(name+'.wasm');wat=a.out/(name+'.wat');wat.write_text('(module '+('(import "p" "nop" (func)) (import "p" "int" (tag $import (param i32))) ' if name.startswith('imported-') else '')+TAGS+' (func '+sig+' '+body+') (func (export "_start")))');subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(path)],check=True)
  for mode,cmd,expect in [('wasmtime',[str(a.wasmtime),'-C','cache=n','-W','exceptions=y']+(['--preload','p='+str(provider)] if name.startswith('imported-') else [])+[str(path)],valid),('scoped',[str(a.validator),str(path)],valid),('default',[str(a.validator),str(path),'default'],valid),('disabled',[str(a.validator),str(path),'disabled'],False)]:
   r=subprocess.run(cmd,capture_output=True,timeout=30);log=r.stdout+r.stderr;(a.out/(name+'-'+mode+'.log')).write_bytes(log);rows.append(dict(case=name,mode=mode,valid=expect,exit=r.returncode,command=cmd));(a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert (r.returncode==0)==expect,(name,mode,log)
  if name.startswith(('catch-ref-tagged-','catch-all-ref-')) or name.startswith('catch-ref-wrong-'):
   for mode,cmd,expect in [('no-function-references',[str(a.validator),str(path),'no-function-references'],valid),
                            ('no-reference-types',[str(a.validator),str(path),'no-reference-types'],False),
                            ('no-exceptions',[str(a.validator),str(path),'no-exceptions'],False)]:
    r=subprocess.run(cmd,capture_output=True,timeout=30);log=r.stdout+r.stderr;(a.out/(name+'-'+mode+'.log')).write_bytes(log);rows.append(dict(case=name,mode=mode,valid=expect,exit=r.returncode,command=cmd));(a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert (r.returncode==0)==expect,(name,mode,log)
 # Latest EH malformed immediates are generated as binary so the WAT assembler cannot normalize them.
 if not a.focus_exnref:
  from run_external_value_cli import leb
 def section(i,data):return bytes([i])+leb(len(data))+data
 bad=[] if a.focus_exnref else [('tag-overflow',b'\x08\x80\x80\x80\x80\x10\x0b'),('tag-truncated',b'\x08\x80'),
      ('tag-unknown-u32max',b'\x08\xff\xff\xff\xff\x0f\x0b'),
      ('label-overflow',b'\x1f\x40\x01\x02\x80\x80\x80\x80\x10\x0b\x0b'),
      ('catch-tag-overflow',b'\x1f\x40\x01\x00\x80\x80\x80\x80\x10\x00\x0b\x0b'),
      ('literal-catch-kind',b'\x1f\x40\x01\x80\x00\x00\x0b\x0b'),
      ('padded-negative-blocktype',b'\x1f\xff\x7f\x00\x00\x0b\x0b')]
 for name,code in bad:
  body=b'\0'+code;path=a.out/('bad-binary-'+name+'.wasm');path.write_bytes(b'\0asm\1\0\0\0'+section(1,b'\1\x60\0\0')+section(3,b'\1\0')+section(13,b'\1\0\0')+section(7,b'\1\6_start\0\0')+section(10,b'\1'+leb(len(body))+body))
  for mode,cmd in [('wasmtime',[str(a.wasmtime),'-C','cache=n','-W','exceptions=y',str(path)]),('scoped',[str(a.validator),str(path)]),('default',[str(a.validator),str(path),'default']),('disabled',[str(a.validator),str(path),'disabled'])]:
   r=subprocess.run(cmd,capture_output=True,timeout=30);log=r.stdout+r.stderr;(a.out/('bad-binary-'+name+'-'+mode+'.log')).write_bytes(log);rows.append(dict(case='bad-binary-'+name,mode=mode,valid=False,exit=r.returncode,command=cmd));(a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert r.returncode!=0,(name,mode,log)
 (a.out/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(selected)+len(bad),runs=len(rows),focus_exnref=a.focus_exnref))+'\n');print('PASS Core 3 production pure exception validator:',len(selected)+len(bad),'new-grammar cases,',len(rows),'comparisons')
if __name__=='__main__':main()

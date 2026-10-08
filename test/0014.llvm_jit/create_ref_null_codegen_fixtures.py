#!/usr/bin/env python3
"""Paired real modules: ref.null of a concrete function heap versus the abstract func heap."""
import argparse,subprocess,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.output.mkdir(parents=True,exist_ok=False)
CASES={
 'identity':'''(module (type $t (func)) (func $get (result funcref) ref.null $t)
  (func (export "_start") call $get ref.is_null i32.eqz if unreachable end))''',
 'tuple':'''(module (type $t (func)) (func $get (result i32 funcref i64) i32.const 7 ref.null $t i64.const 9)
  (func (export "_start") call $get i64.const 9 i64.ne if unreachable end ref.is_null i32.eqz if unreachable end i32.const 7 i32.ne if unreachable end))''',
 'indirect':'''(module (type $t (func)) (type $sig (func (result funcref))) (table 1 funcref) (elem (i32.const 0) $get)
  (func $get (type $sig) ref.null $t) (func (export "_start") i32.const 0 call_indirect (type $sig) ref.is_null i32.eqz if unreachable end))''',
 'memory':'''(module (type $t (func)) (memory 1) (func $get (param i32) (result funcref i32)
  local.get 0 i32.const 42 i32.store ref.null $t local.get 0 i32.load)
  (func (export "_start") i32.const 65532 call $get i32.const 42 i32.ne if unreachable end ref.is_null i32.eqz if unreachable end))''',
}
for name,wat in CASES.items():
 for suffix,source in [('',wat),('-short',wat.replace('ref.null $t','ref.null func'))]:
  path=a.output/(name+suffix+'.wat');path.write_text(source);subprocess.run([str(a.wasm_tools),'parse',str(path),'-o',str(path.with_suffix('.wasm'))],check=True)
(a.output/'description.json').write_text(json.dumps(dict(feature='ref.null concrete function heap',comparison='same signatures and bodies except the null heap immediate'),indent=2)+'\n')

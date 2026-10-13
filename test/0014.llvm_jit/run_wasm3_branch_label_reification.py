#!/usr/bin/env python3
"""Reject typed-reference branch fallthrough that lost the label's declared supertype."""
import argparse, hashlib, json, resource, subprocess
from pathlib import Path
CASES=('br_if','br_on_null','br_on_non_null')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--uwvm',type=Path,required=True);ap.add_argument('--wasm-tools',type=Path,required=True)
    ap.add_argument('--fixtures',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--source-id',required=True);a=ap.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=True)
    rows=[]
    for opcode in CASES:
        for status in ('invalid','valid'):
            stem=f'branch_label_{opcode}_{status}'
            wat=a.fixtures/f'{stem}.wat';wasm=a.out/f'{stem}.wasm'
            parse=subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],capture_output=True,timeout=30)
            assert parse.returncode==0,(stem,parse.stderr)
            oracle=subprocess.run([str(a.wasm_tools),'validate','--features','all',str(wasm)],capture_output=True,timeout=30)
            actual=subprocess.run([str(a.uwvm),'-m','validation','-WFE-function-references','--run',str(wasm)],capture_output=True,timeout=30)
            (a.out/f'{stem}-oracle.log').write_bytes(oracle.stdout+oracle.stderr)
            (a.out/f'{stem}-uwvm.log').write_bytes(actual.stdout+actual.stderr)
            expected=status=='valid'
            passed=(oracle.returncode==0)==expected and (actual.returncode==0)==expected
            rows.append({'case':stem,'expected_valid':expected,'passed':passed,
                         'wasm_sha256':sha(wasm),'oracle_exit':oracle.returncode,'uwvm_exit':actual.returncode})
            if not passed:break
        if not all(x['passed'] for x in rows):break
    summary={'passed':len(rows)==6 and all(x['passed'] for x in rows),'source_id':a.source_id,
             'uwvm_sha256':sha(a.uwvm),'wasm_tools_sha256':sha(a.wasm_tools),'checks':len(rows),'rows':rows}
    (a.out/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='rows'}),flush=True)
    if not summary['passed']:raise SystemExit(1)
if __name__=='__main__':main()

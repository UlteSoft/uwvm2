#!/usr/bin/env python3
"""Audit saved native memory64 load/store objects, excluding size/grow bridges."""
import argparse,json,re
from pathlib import Path
from check_wasm3_tail_codegen import success_path_calls

def audit(assembly, expected=112, atomic_cases=None):
    assert 'file format elf64-x86-64' in assembly
    chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);handlers=[]
    for i in range(1,len(chunks),3):
        name,body=chunks[i+1:i+3]
        if not re.fullmatch(r'access_[0-9]+',name):continue
        returns=bool(re.search(r'\bretq?\b',body))
        hot_calls=success_path_calls(body) if returns else 0
        assert hot_calls==0,(name,'helper call on successful native access')
        if not returns:assert re.search(r'\bcallq?\b',body),(name,'no success or trap path')
        if atomic_cases is not None:
            case=atomic_cases[name]
            assert not re.search(r'\b(?:mfence|lfence|sfence)\b',body),(name,'redundant fence')
            if returns and case['store']:
                assert len(re.findall(r'\bxchg[bwlq]\s+[^\n]*\(',body))==1,(name,'not one native sequentially consistent store')
        handlers.append(dict(name=name,success_path=returns,success_calls=hot_calls))
    assert len(handlers)==expected,len(handlers)
    return handlers

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('assembly',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();rows=audit(a.assembly.read_text());a.output.write_text(json.dumps(rows,indent=2)+'\n')
    print('PASS 112 actual LLVM memory64 functions: zero helper calls on successful access paths')

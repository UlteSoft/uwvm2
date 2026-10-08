#!/usr/bin/env python3
"""Grouped producer reference postfix DATA against an actual supplied C++ ELF."""
from pathlib import Path
import hashlib,importlib.util,json,random,subprocess,sys,time
sys.dont_write_bytecode=True
def verify(root,command,out,old_adapter=None):
    root,out=Path(root),Path(out);assert sys.platform=="linux"
    subprocess.run(["bash",str(root/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
    out.mkdir(exist_ok=False);pins={}
    def pin(p):
        p=Path(p);pins[str(p)]=hashlib.file_digest(p.open("rb"),"sha256").hexdigest()
    def module(p,name):
        spec=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
    pin(__file__);p=root/"tools/debug/dap_adapter.py";pin(p);dap=module(p,"fixed_grouped")
    old=module(old_adapter,"old_grouped") if old_adapter else None
    if old_adapter:pin(old_adapter)
    rng=random.Random(20261007)
    def signature(name,steps):
        value=2166136261
        def add(byte):
            nonlocal value
            value=((value^byte)*16777619)&0xffffffff
        for byte in name.encode():add(byte)
        add(255)
        for kind,data in steps:
            add(kind)
            if kind==0:
                for byte in data.encode():add(byte)
                add(254)
            elif kind==1:
                for byte in (data&((1<<64)-1)).to_bytes(8,"little"):add(byte)
        return value
    cases=[]
    for i in range(1024):
        root_name=rng.choice(["p","packet","array","pair","ns::packet"])
        text=root_name;steps=[]
        for j in range(rng.randrange(1,9)):
            shape=rng.randrange(4)
            if shape==0:
                name=rng.choice(["field","next","value","0","1"]);suffix="."+name;extra=[(0,name)]
            elif shape==1:
                name=rng.choice(["field","next","value","0"]);suffix="->"+name;extra=[(2,None),(0,name)]
            elif shape==2:
                index=rng.randrange(-5,6);suffix="["+str(index)+"]";extra=[(1,index)]
            else:suffix=".*";extra=[(2,None)]
            if len(steps)+len(extra)>32:break
            text="("+text+")"+suffix;steps+=extra
        # Alternate safe numeric contexts; each read uses the complete path.
        mode=i%4
        if mode==0:expression=text+" + 7";bits,reads=49,1
        elif mode==1:expression="2 * ("+text+") - 5";bits,reads=79,1
        elif mode==2:expression="0 && ("+text+" + missing)";bits,reads=0,0
        else:expression="1 || ("+text+" + missing)";bits,reads=1,0
        cases.append({"expression":expression,"accepted":True,"bits":bits,"reads":reads,"signature":signature(root_name,steps) if reads else 0})
    # Combined step bounds: the outer tail must count existing child steps.
    for count in [30,31,32,33]:
        text="(packet"+".field"*count+").field + 1"
        if len(text)>256:continue
        cases.append({"expression":text,"accepted":count<32,"bits":43,"reads":1,
                      "signature":signature("packet",[(0,"field")]*(count+1))})
    invalid=["(p+1)->field + 1","(42).field + 1","(int(p)).field + 1","(true).field + 1","(sizeof(p)).field + 1",
        "(p as i32).field + 1","(1 ? p : p).field + 1","*(p+1)","(packet)[1+1] + 1","(packet). * + 1",
        "(packet)-> + 1","(packet)[9223372036854775808] + 1","(packet)[-9223372036854775809] + 1",
        "(packet)[01] + 1","(packet).field = 1","(packet).field++","(ns ::packet).field + 1","(ns:: packet).field + 1"]
    # Canonical leading-zero indices are a DAP-only prior restriction.
    invalid_cpp=[x for x in invalid if "[01]" not in x]
    cases += [{"expression":text,"accepted":False} for text in invalid_cpp]
    assert all(len(x["expression"])<=256 for x in cases)
    record={"passed":False,"cases":len(cases),"rows":[],"inputs":pins,"old_DAP_refusals":0,
        "scope":__doc__,"cgroup":Path("/proc/self/cgroup").read_text(),"live_native_language_parity":False}
    corpus=out/"cases.json";corpus.write_text(json.dumps(cases,separators=(",",":"))+"\n");pin(corpus)
    for row in cases:
        try:dap.validate_source_evaluation_expression(row["expression"]);accepted=True
        except ValueError:accepted=False
        assert accepted==row["accepted"],("DAP mismatch",row,accepted)
        if old and row["accepted"]:
            try:old.validate_source_evaluation_expression(row["expression"])
            except ValueError:record["old_DAP_refusals"]+=1
    for text in invalid:
        try:dap.validate_source_evaluation_expression(text)
        except ValueError:continue
        raise AssertionError(("invalid suffix accepted",text))
    if old:assert record["old_DAP_refusals"]>0
    for start in range(0,len(cases),128):
        batch=cases[start:start+128];log=out/("batch-"+str(start)+".log");began=time.monotonic()
        with log.open("wb") as f:r=subprocess.run([*command,*[v["expression"] for v in batch]],stdout=f,stderr=subprocess.STDOUT,timeout=90)
        assert r.returncode==0,("target failed",r.returncode,log.read_text()[-2000:])
        lines=log.read_text().splitlines();assert len(lines)==len(batch)*2
        for i,expected in enumerate(batch):
            for j,guest in enumerate([32,64]):
                v=list(map(int,lines[2*i+j].split("\t")));assert v[:3]==[i,guest,int(expected["accepted"])],("parse mismatch",expected,v)
                if expected["accepted"]:
                    assert v[3:8]==[0,expected["bits"],32,0,expected["reads"]],("value mismatch",expected,v)
                    assert v[8:]==[0,expected["signature"]],("read plan mismatch",expected,v)
                else:assert v[7:]==[0,0,0],"invalid path gained resolver calls"
        pin(log);record["rows"].append({"start":start,"cases":len(batch),"actual_CXX_rows":len(lines),"seconds":time.monotonic()-began,"log":str(log),"sha256":pins[str(log)]})
    assert all(hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()==h for p,h in pins.items())
    record.update(passed=True,inputs_after_unchanged=True)
    (out/"results.json").write_text(json.dumps(record,indent=2)+"\n")
    return record

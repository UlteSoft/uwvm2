#!/usr/bin/env python3
"""Exact rational floating literal/DAP qualification against a supplied C++ ELF.

Owned DATA only: no real stop, producer session, pointer or VM memory access.
The caller must execute inside the existing guarded Linux cgroup.
"""
from pathlib import Path
import decimal,hashlib,importlib.util,json,random,subprocess,sys,time
sys.dont_write_bytecode=True

def verify(root,command,out,old_adapter=None):
    root,out=Path(root),Path(out)
    assert sys.platform=="linux" and len(command)>0
    subprocess.run(["bash",str(root/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
    out.mkdir(exist_ok=False)
    pins={}
    def pin(p):
        p=Path(p);pins[str(p)]=hashlib.file_digest(p.open("rb"),"sha256").hexdigest()
    pin(__file__);adapter=root/"tools/debug/dap_adapter.py";pin(adapter)
    def load(p,name):
        spec=importlib.util.spec_from_file_location(name,p)
        m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
    dap=load(adapter,"fixed_float_range")
    old=load(old_adapter,"old_float_range") if old_adapter else None
    if old_adapter:pin(old_adapter)
    cutoff=(1<<128)-(1<<103)
    # Independently encode exact nonnegative rationals into IEEE nearest-even.
    # No float(), Decimal arithmetic, production parser or regex supplies bits.
    def bits(n,d,width):
        mb,eb=(23,127) if width==32 else (52,1023)
        if not n:return 0
        e=n.bit_length()-d.bit_length()
        if (n < d<<e if e>=0 else n<<-e < d):e-=1
        e=max(e,1-eb);shift=mb-e
        numerator,denominator=(n<<shift,d) if shift>=0 else (n,d<<-shift)
        q,r=divmod(numerator,denominator)
        q+=int(2*r>denominator or 2*r==denominator and q%2)
        if q==1<<(mb+1):q>>=1;e+=1
        if e>eb or q==0:return None
        if q<1<<mb:return q
        return ((e+eb)<<mb)|(q-(1<<mb))
    def literal(n,scale,style,suffix,separator):
        digits=str(n)
        if style==0:
            text=digits+".0" if scale==0 else digits+"e"+str(-scale)
        elif style==1:
            text=digits[0]+"."+digits[1:]+"e"+str(len(digits)-1-scale)
        else:
            text="."+digits+"e"+str(len(digits)-scale)
        if separator:
            text="".join(c+(separator if i+1<len(text) and c.isdigit() and text[i+1].isdigit() else "")
                         for i,c in enumerate(text))
        return text+suffix
    cases=[]
    rng=random.Random(20261006)
    samples=[(cutoff+i,0) for i in [-1000000000,-65536,-1,0,1,65536,1000000000]]
    samples += [(cutoff*10**scale+i,scale) for scale in [1,10,30,60,90]
                for i in [-65536,-1,0,1,65536]]
    samples += [(5**150+i,150) for i in [-65536,-1,0,1,65536]]
    samples += [(rng.randrange(1,10**rng.randrange(1,50)),rng.randrange(-50,100)) for _ in range(96)]
    samples += [(0,-999),(1,999),((1<<24)-1,0),(1,45),(5,46),(7,46),(8,46)]
    for n,scale in samples:
        num,den=(n,10**scale) if scale>=0 else (n*10**-scale,1)
        for style in range(3):
            for suffix in ["f","F",""]:
                width=32 if suffix else 64;expected=bits(num,den,width)
                separator=["","_","'"][(style+len(str(n)))%3]
                text=literal(n,scale,style,suffix,separator)
                if len(text)>220:continue
                cases.append({"expression":text,"accepted":expected is not None,"bits":expected,"width":width})
    # A second group catches dead-branch syntax and preserves common numeric
    # expression behavior using bounded native arithmetic.
    invalid=["3.5e38f","1e39F","3.5_0e3_8f",str(cutoff)+".0f"]
    wrapped=[v for text in invalid for v in [text,"0 && "+text,"1 || "+text,"1 ? 7.0 : "+text]]
    # Include literals whose binary64 images equal the exact cutoff from below.
    just_below=[v for v in cases if v["accepted"] and v["width"]==32 and v["bits"]==0x7f7fffff]
    record={"passed":False,"scope":__doc__,"cases":len(cases),"rows":[],"inputs":pins,
            "cgroup":Path("/proc/self/cgroup").read_text(),"old_false_acceptances":0,
            "valid_maximum_rounding_cases":len(just_below),"full_native_language_parity":False}
    corpus=out/"generated-cases.json";corpus.write_text(json.dumps(cases,separators=(",",":"))+"\n");pin(corpus)
    # Construction/comparison must not depend on ambient Decimal precision,
    # rounding, exponent bounds, traps or sticky flags.
    ctx=decimal.getcontext().copy()
    try:
        decimal.getcontext().prec=1;decimal.getcontext().Emax=1;decimal.getcontext().Emin=-1
        decimal.getcontext().rounding=decimal.ROUND_UP
        for signal in decimal.getcontext().traps:decimal.getcontext().traps[signal]=True
        initial=dict(decimal.getcontext().flags)
        for row in cases:
            try:dap.validate_source_evaluation_expression(row["expression"]);accepted=True
            except ValueError:accepted=False
            assert accepted==row["accepted"],("DAP finite range disagreement",row,accepted)
            if old and not row["accepted"] and row["width"]==32:
                try:old.validate_source_evaluation_expression(row["expression"]);record["old_false_acceptances"]+=1
                except ValueError:pass
        assert dict(decimal.getcontext().flags)==initial,"validator mutated Decimal flags"
    finally:decimal.setcontext(ctx)
    for text in wrapped:
        try:dap.validate_source_evaluation_expression(text)
        except ValueError:continue
        raise AssertionError(("overflowed dead syntax accepted",text))
    if old:assert record["old_false_acceptances"]>0
    for start in range(0,len(cases),256):
        batch=cases[start:start+256];log=out/("batch-"+str(start)+".log")
        argv=[*command,*[v["expression"] for v in batch]]
        began=time.monotonic()
        with log.open("wb") as f:r=subprocess.run(argv,stdout=f,stderr=subprocess.STDOUT,timeout=90)
        assert r.returncode==0,("target failed",r.returncode,log.read_text()[-2000:])
        lines=log.read_text().splitlines();assert len(lines)==2*len(batch)
        for i,expect in enumerate(batch):
            for j,guest in enumerate([32,64]):
                fields=list(map(int,lines[2*i+j].split("\t")))
                assert fields[:3]==[i,guest,int(expect["accepted"])],("C++ finite range disagreement",expect,fields)
                if expect["accepted"]:assert fields[3:6]==[expect["bits"],expect["width"],1],("C++ rational IEEE disagreement",expect,fields)
                assert fields[6:]==[0,0],"literal obtained resolver authority"
        pin(log);record["rows"].append({"start":start,"cases":len(batch),"CXX_result_rows":len(lines),
            "returncode":r.returncode,"seconds":time.monotonic()-began,"log":str(log),"sha256":pins[str(log)]})
    record["wrapped_overflow_rejections"]=len(wrapped)
    assert all(hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()==h for p,h in pins.items())
    record.update(passed=True,inputs_after_unchanged=True)
    (out/"results.json").write_text(json.dumps(record,indent=2)+"\n")
    return record

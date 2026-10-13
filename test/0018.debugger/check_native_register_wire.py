#!/usr/bin/env python3
"""Parse actual C++ projection/formatter output; DATA, not native OS evidence."""
import importlib.util
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("wire_dap",root/"tools/debug/dap_adapter.py")
dap=importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
text=Path(sys.argv[1]).read_text()
assert text.endswith("native-registers end\n")
seen=[]
location={"stop_id":17,"id":3,"module":0,"function":2,"generation":8,"native_pc":"0x1234"}
for chunk in text.split("native-registers end\n")[:-1]:
    architecture,rows=dap.parse_native_registers(chunk+"native-registers end\n",location)
    width=int(chunk.splitlines()[0].split("word-bits=")[1])
    seen.append((architecture,width))
    assert all("memoryReference" not in row and row["variablesReference"]==0 for row in rows)
    assert all("decafbad" not in row["value"] for row in rows), "unproved upper host bits escaped"
    assert any(row["value"] not in ("unavailable","0x0000000000001234","0x00001234") for row in rows)
assert seen==[("x86_64",64),("aarch64",64),("i686",32),("powerpc",32),("powerpc",64),
              ("mips64",64),("riscv64",64),("loongarch64",64),("sparc64",64),("s390x",64),("arm",32)],seen
print("PASS 11 actual C++ formatter/projection DATA -> DAP layouts; no native OS qualification")

#!/usr/bin/env python3
"""Prepare an immutable old/new codec comparison; baseline is a pre-edit source snapshot."""
import argparse,hashlib,json,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[3];a.out.mkdir(parents=True,exist_ok=False)
files=['src/uwvm2/utils/hash/xxh3.h','src/uwvm2/runtime/llvm_jit_cache/format.h','src/uwvm2/runtime/llvm_jit_cache/compress.h',
       'src/uwvm2/runtime/compiler/shared/strict_float.h',
       'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h']
manifest={}
for version,source in [('before',a.baseline),('after',root)]:
 for name in files:
  data=(source/name).read_bytes();target=a.out/version/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
  manifest[version+'/'+name]=hashlib.sha256(data).hexdigest()
 text=(source/files[-1]).read_text();start=text.index('template <typename UInt>\n[[nodiscard]] inline constexpr bool parse_wasm_little_endian_immediate(')
 end=text.index('\n}\n',start)+3
 (a.out/version/'immediate.h').write_text(text[start:end])
(a.out/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')

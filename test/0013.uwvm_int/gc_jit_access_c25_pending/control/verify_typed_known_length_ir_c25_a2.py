from pathlib import Path
import sys,json,re,ctypes
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/typed-known-length-ir-witness-c25-a1';inp=D/'fragment.ll';opt=D/'optimized.ll';text=opt.read_text();oldopt=json.loads((R/'evidence/c25-known-length-fragment-opt-a1.json').read_text());assert oldopt['returncode']==0 and oldopt['guard'] is None
blocks={name:re.search(r'define[^@]*@'+name+r'\([^\n]*\)[^{]*\{(.*?)^\}',text,re.S|re.M).group(1) for name in ['c23_original','c25_known','c25_unknown','c25_dynamic']}
assert 'native.length = load i64' not in blocks['c25_known'] and not re.search(r'icmp \w+ i64',blocks['c25_known'])
for name in ['c23_original','c25_unknown','c25_dynamic']:assert 'native.length = load i64' in blocks[name] and re.search(r'icmp \w+ i64',blocks[name])
for name,b in blocks.items():assert '@authenticate' in b and 'atomicrmw xchg' in b and 'store atomic i8 0' in b
lib=Path('/usr/lib/x86_64-linux-gnu/libLLVM.so.22.1');original_sha=N.sha(lib)
with N.lease():
 L=ctypes.CDLL(str(lib));vp=ctypes.c_void_p;cp=ctypes.c_char_p;ui=ctypes.c_uint
 def sig(name,args,result):
  f=getattr(L,name);f.argtypes=args;f.restype=result;return f
 sig('LLVMGetVersion',[ctypes.POINTER(ui)]*3,None);version=[ui() for _ in range(3)];L.LLVMGetVersion(*[ctypes.byref(v) for v in version])
 sig('LLVMContextCreate',[],vp);sig('LLVMContextDispose',[vp],None);sig('LLVMCreateMemoryBufferWithContentsOfFile',[cp,ctypes.POINTER(vp),ctypes.POINTER(cp)],ctypes.c_int);sig('LLVMParseIRInContext',[vp,vp,ctypes.POINTER(vp),ctypes.POINTER(cp)],ctypes.c_int)
 sig('LLVMGetTargetFromTriple',[cp,ctypes.POINTER(vp),ctypes.POINTER(cp)],ctypes.c_int);sig('LLVMCreateTargetMachine',[vp,cp,cp,cp,ctypes.c_int,ctypes.c_int,ctypes.c_int],vp)
 sig('LLVMSetTarget',[vp,cp],None);sig('LLVMCreateTargetDataLayout',[vp],vp);sig('LLVMCopyStringRepOfTargetData',[vp],vp);sig('LLVMSetDataLayout',[vp,cp],None);sig('LLVMDisposeTargetData',[vp],None);sig('LLVMDisposeMessage',[vp],None);sig('LLVMDisposeTargetMachine',[vp],None);sig('LLVMDisposeModule',[vp],None);sig('LLVMTargetMachineEmitToFile',[vp,vp,cp,ctypes.c_int,ctypes.POINTER(cp)],ctypes.c_int)
 rows=[]
 for triple,cpu in [('x86_64-linux-gnu','alderlake'),('aarch64-linux-gnu','neoverse-v1')]:
  p=D/(triple+'.s');r=json.loads((R/'evidence'/('c25-known-length-fragment-llc-a1-'+triple+'.json')).read_text());assert r['returncode']==0 and r['guard'] is None and p.stat().st_size>0;rows.append(dict(target=triple,cpu=cpu,backend='native bundled LLVM23 llc',assembly_sha=N.sha(p),receipt=r['label']))
 for backend,triple,cpu in [('PowerPC','powerpc64le-linux-gnu','pwr9'),('Mips','mips64el-linux-gnuabi64','mips64r2'),('LoongArch','loongarch64-linux-gnu','la464'),('Sparc','sparcv9-linux-gnu','generic')]:
  N.check()
  for part in ['TargetInfo','Target','TargetMC','AsmPrinter']:sig('LLVMInitialize'+backend+part,[],None)()
  ctx=L.LLVMContextCreate();buf=vp();module=vp();err=cp();assert L.LLVMCreateMemoryBufferWithContentsOfFile(str(opt).encode(),ctypes.byref(buf),ctypes.byref(err))==0,err.value;assert L.LLVMParseIRInContext(ctx,buf,ctypes.byref(module),ctypes.byref(err))==0,err.value
  target=vp();assert L.LLVMGetTargetFromTriple(triple.encode(),ctypes.byref(target),ctypes.byref(err))==0,err.value;tm=L.LLVMCreateTargetMachine(target,triple.encode(),cpu.encode(),b'',3,0,0);assert tm
  L.LLVMSetTarget(module,triple.encode());td=L.LLVMCreateTargetDataLayout(tm);dl=L.LLVMCopyStringRepOfTargetData(td);L.LLVMSetDataLayout(module,ctypes.cast(dl,cp));L.LLVMDisposeMessage(dl);L.LLVMDisposeTargetData(td)
  asm=D/(triple+'-llvm22.s');assert not asm.exists();name=ctypes.create_string_buffer(str(asm).encode());assert L.LLVMTargetMachineEmitToFile(tm,module,ctypes.cast(name,cp),0,ctypes.byref(err))==0,err.value;assert asm.stat().st_size>0;N.persist(asm);rows.append(dict(target=triple,cpu=cpu,backend='installed Ubuntu libLLVM22 CAPI',assembly_sha=N.sha(asm),library_sha=original_sha));L.LLVMDisposeTargetMachine(tm);L.LLVMDisposeModule(module);L.LLVMContextDispose(ctx);print('C25_CROSS_CODEGEN',triple,flush=True)
 assert N.sha(lib)==original_sha
 N.save(D/'witness-a2.json',dict(input_sha=N.sha(inp),optimized_sha=N.sha(opt),blocks={k:dict(header_length_load='native.length = load i64' in b,range_test=bool(re.search(r'icmp \w+ i64',b)),auth_retained='@authenticate' in b,lock_retained='atomicrmw xchg' in b) for k,b in blocks.items()},targets=rows,worker_sha=N.sha(__file__),cross_library=str(lib),cross_library_sha=original_sha,cross_llvm_version=[v.value for v in version],scope='Synthetic isolated LLVM fragment, NOT actual UWVM2-generated IR, NOT full candidate build/correctness, NOT native or cross-platform performance. BundledLLVM23 optimizer eliminates constant-known header length load+rangecheck; unknown retains, auth+locks remain. x86/AArch nativeLLVM23 codegen; four extra targets use installedLLVM22 CAPI because bundledLLVM23 contains only two targets. Platformeligibility unchanged; no downloads/system installation.'))
 print('C25_IR_WITNESS_DONE',len(rows),flush=True)

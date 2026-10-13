from pathlib import Path
import sys,json,re
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/typed-known-length-ir-witness-c25-a1';B=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin')
def function(name,length,index):
 lines=['define i32 @'+name+'(ptr %opaque, i32 %n, i32 %known, i1 %valid) {','entry:','  %header = call ptr @authenticate(ptr %opaque)','  %length.ptr = getelementptr i8, ptr %header, i64 48','  %native.length = load i64, ptr %length.ptr, align 8','  %known64 = zext i32 %known to i64',length,'  %bounded = and i32 %n, 1023','  %index = zext i32 %bounded to i64','  %bad = icmp uge i64 %index, %length','  br i1 %bad, label %trap, label %access','trap:','  call void @llvm.trap()','  unreachable','access:','  %data.ptr = getelementptr i8, ptr %header, i64 40','  %data = load ptr, ptr %data.ptr, align 8','  %element = getelementptr i32, ptr %data, i64 %index','  %flag = getelementptr i8, ptr %header, i64 32','  br label %lock','lock:','  %old = atomicrmw xchg ptr %flag, i8 1 acquire, align 1','  %busy = icmp ne i8 %old, 0','  br i1 %busy, label %lock, label %read','read:','  %value = load i32, ptr %element, align 1','  store atomic i8 0, ptr %flag release, align 1','  ret i32 %value','}']
 return '\n'.join(lines)+'\n'
ir='declare ptr @authenticate(ptr) nounwind\ndeclare void @llvm.trap() cold noreturn nounwind\n'
ir+=function('c23_original','  %length = add i64 %native.length, 0',0)
ir+=function('c25_known','  %length = select i1 true, i64 1024, i64 %native.length',0)
ir+=function('c25_unknown','  %length = select i1 false, i64 %known64, i64 %native.length',0)
ir+=function('c25_dynamic','  %length = select i1 %valid, i64 %known64, i64 %native.length',0)
with N.lease():
 D.mkdir(exist_ok=False);p=D/'fragment.ll';p.write_text(ir);N.persist(p);opt=D/'optimized.ll'
 N.run('c25-known-length-fragment-opt-a1',[B/'opt','-passes=default<O3>','-S',p,'-o',opt],cpu=20,timeout=30);N.persist(opt);t=opt.read_text()
 blocks={name:re.search(r'define[^@]*@'+name+r'\([^\n]*\)[^{]*\{(.*?)^\}',t,re.S|re.M).group(1) for name in ['c23_original','c25_known','c25_unknown','c25_dynamic']}
 assert 'native.length = load i64' not in blocks['c25_known'] and not re.search(r'icmp \w+ i64',blocks['c25_known'])
 for name in ['c23_original','c25_unknown','c25_dynamic']:assert 'native.length = load i64' in blocks[name] and re.search(r'icmp \w+ i64',blocks[name]),name
 for name,b in blocks.items():assert '@authenticate' in b and 'atomicrmw xchg' in b and 'store atomic i8 0' in b,name
 targets=[('x86_64-linux-gnu','alderlake'),('aarch64-linux-gnu','neoverse-v1'),('powerpc64le-linux-gnu','pwr9'),('mips64el-linux-gnuabi64','mips64r2'),('loongarch64-linux-gnu','la464'),('sparcv9-linux-gnu','generic')]
 rows=[]
 for triple,cpu in targets:
  asm=D/(triple+'.s');lab='c25-known-length-fragment-llc-a1-'+triple
  N.run(lab,[B/'llc','-O=3','-mtriple='+triple,'-mcpu='+cpu,opt,'-o',asm],cpu=20,timeout=30);N.persist(asm);rows.append(dict(target=triple,cpu=cpu,assembly_sha=N.sha(asm)))
 N.save(D/'witness.json',dict(input_sha=N.sha(p),optimized_sha=N.sha(opt),blocks={k:dict(header_length_load='native.length = load i64' in b,range_test=bool(re.search(r'icmp \w+ i64',b)),auth_retained='@authenticate' in b,lock_retained='atomicrmw xchg' in b) for k,b in blocks.items()},targets=rows,worker_sha=N.sha(__file__),scope='Synthetic isolated LLVM23 fragment matching proposed C25 length-selection and existing numeric-read lock structure. Not actual UWVM2-generated IR or performance, not fullC25 compilation/correctness. Nativeopt proves known constant removes load+rangecheck, unknown retains both, all auth+locks remain. Cross-target assembly is codegen inspection only; current runtime typed-array eligibility/platform gates unchanged.'))
 print('C25_FRAGMENT_VERIFIED',len(rows),flush=True)

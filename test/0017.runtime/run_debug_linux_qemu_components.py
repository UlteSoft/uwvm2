#!/usr/bin/env python3
"""Actual Linux QEMU component regression; does not qualify full language/ASM CLI parity.
Run only behind the original cgroup owned-process supervisor. All assertions
use production parsers/policies, with finite DATA providers rather than VM leases.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,re,shlex,struct,subprocess,sys,time
sys.dont_write_bytecode=True
PROFILES=[
 ('x86_64','x86_64-linux-gnu','x86_64',62,64,'little',[]),
 ('aarch64','aarch64-linux-gnu','aarch64',183,64,'little',[]),
 ('i686','i686-linux-gnu','i386',3,32,'little',['-msse2']),
 ('riscv64','riscv64-linux-gnu','riscv64',243,64,'little',['-march=rv64gc','-mabi=lp64d']),
 ('ppc64','powerpc64-linux-gnu','ppc64',21,64,'big',['-mcpu=power8']),
 ('ppc64le','powerpc64le-linux-gnu','ppc64le',21,64,'little',['-mcpu=power8']),
 ('ppc32','powerpc-linux-gnu','ppc',20,32,'big',[]),
 ('mips64','mips64-linux-gnuabi64','mips64',8,64,'big',['-march=mips64r2','-mabi=64']),
 ('mips64el','mips64el-linux-gnuabi64','mips64el',8,64,'little',['-march=mips64r2','-mabi=64']),
 ('mips32','mips-linux-gnu','mips',8,32,'big',['-march=mips32r2','-mabi=32']),
 ('mips32el','mipsel-linux-gnu','mipsel',8,32,'little',['-march=mips32r2','-mabi=32']),
 ('sparc64','sparc64-linux-gnu','sparc64',43,64,'big',[]),
 ('loongarch64','loongarch64-linux-gnu','loongarch64',258,64,'little',['-march=la464']),
 ('s390x','s390x-linux-gnu','s390x',22,64,'big',['-march=z14']),
 ('armhf','arm-linux-gnueabihf','arm',40,32,'little',['-mcpu=cortex-a15','-mfpu=neon-vfpv4']),
 ('armel','arm-linux-gnueabi','arm',40,32,'little',['-mcpu=arm926ej-s','-mfloat-abi=soft']),
]
CASES=[
 'debug_source_size_extent', 'debug_source_size_type',
 'debug_source_logical_preflight','debug_source_utf_character_literal','debug_source_rust_character_literal','debug_source_rust_numeric_literal','debug_source_condition_truth','debug_source_character_escape','debug_source_sizeof_unary', 'debug_source_sizeof_expression', 'debug_source_integer_rank', 'debug_source_narrow_bridge','debug_source_exact_narrow','debug_source_language_numeric','debug_source_language_predicate','debug_source_zig_producer','debug_source_conditional_distinct','debug_source_conditional_expression','debug_source_run_to','debug_source_step_policy','debug_source_map_v3','debug_source_scalar_expression','debug_source_scalar_property','debug_source_tuple_postfix','debug_source_dap_expression','debug_source_dap_reply','debug_source_dwarf_expression','debug_source_dwarf_selectors','debug_wasm_state_view','debug_wasm_mutation_data','debug_source_language_expression','debug_source_string_preview','debug_source_sequence_provenance','debug_source_character_literal','debug_source_zig_typeproof','debug_source_zig_coercion','debug_source_zig_preflight','debug_source_zig_negative','debug_source_zig_category','debug_source_boolean_cast','debug_source_float_separator','debug_source_float_range','debug_source_grouped_postfix','debug_source_cast_spacing','debug_source_builtin_type_specifiers','debug_console_displays','native_linux_register_projection','native_boundary','native_kernel_step','debug_checkpoint_codec','debug_checkpoint_dynamic_native_packet','debug_source_dwarf_values','debug_source_dwarf_qualified_values','debug_source_dwarf_transformed_pieces','debug_wasm_memory_mutation_data']
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def elf(p,machine,bits,order):
 data=Path(p).read_bytes();assert data[:4]==b'\x7fELF' and data[4]==(2 if bits==64 else 1) and data[5]==(1 if order=='little' else 2)
 endian='<' if order=='little' else '>';assert struct.unpack_from(endian+'H',data,18)[0]==machine
 phoff=struct.unpack_from(endian+('Q' if bits==64 else 'I'),data,32 if bits==64 else 28)[0]
 size,count=struct.unpack_from(endian+'HH',data,54 if bits==64 else 42);assert size==(56 if bits==64 else 32) and phoff+count*size<=len(data)
 stack_flags=[struct.unpack_from(endian+'I',data,phoff+i*size+(4 if bits==64 else 24))[0] for i in range(count) if struct.unpack_from(endian+'I',data,phoff+i*size)[0]==0x6474e551]
 return {'machine':machine,'bits':bits,'byte_order':order,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'GNU_STACK_flags':stack_flags}
def load_provider_map(path,selected):
 """Explicit profile SDK roots; malformed mappings fail before any target starts."""
 known={p[0] for p in PROFILES}
 if not selected or not selected<=known:raise ValueError('invalid selected profiles')
 with Path(path).open('rb') as stream:data=stream.read(65537)
 if len(data)>65536:raise ValueError('provider map exceeds 64 KiB')
 def unique(pairs):
  result={}
  for key,value in pairs:
   if key in result:raise ValueError('duplicate provider profile: '+key)
   result[key]=value
  return result
 roots=json.loads(data,object_pairs_hook=unique)
 if not isinstance(roots,dict) or set(roots)!=selected:raise ValueError('provider map must name every selected profile exactly once')
 resolved={}
 for profile,value in roots.items():
  if not isinstance(value,str) or not value or not Path(value).is_absolute():raise ValueError('provider root must be an absolute directory: '+profile)
  root=Path(value).resolve(strict=True)
  if not root.is_dir():raise ValueError('provider root is not a directory: '+profile)
  resolved[profile]=root
 return resolved
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
 ap.add_argument('--deps-root',type=Path,default=Path('/home/macromodel/Documents/uwvm3-implementation/deps'),help='Cross sysroots/GCC/link providers; independently pinned in records')
 ap.add_argument('--qemu-root',type=Path,help='QEMU package root; defaults to --deps-root')
 ap.add_argument('--provider-map',type=Path,help='JSON mapping each selected profile to its absolute SDK root; overrides --deps-root per profile')
 ap.add_argument('--profiles',default=','.join(p[0] for p in PROFILES));ap.add_argument('--cases',default=','.join(CASES));ap.add_argument('--repositories',default='uwvm2,uwvm2-ros');a=ap.parse_args()
 selected=set(a.profiles.split(','));cases=a.cases.split(',');repos=a.repositories.split(',')
 assert selected<={p[0] for p in PROFILES} and set(cases)<=set(CASES) and set(repos)<={'uwvm2','uwvm2-ros'}
 provider_sha=sha(a.provider_map) if a.provider_map is not None else None
 provider_map=load_provider_map(a.provider_map,selected) if a.provider_map is not None else {}
 if a.provider_map is not None:assert sha(a.provider_map)==provider_sha,'provider map changed during read'
 a.out.mkdir(parents=True,exist_ok=False)
 subprocess.run(['bash',str(a.root/'uwvm2/tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 D=a.deps_root;Q=a.qemu_root if a.qemu_root is not None else D;clang=Path('/usr/lib/llvm-22/bin/clang++');lld=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/ld.lld')
 record={'passed':False,'scope':'actual target ELF language/checkpoint DATA and isolated kernel backend fixtures; not full uwvm CLI, native hardware, runtime-issued language leases or ASM Wasm guest-code qualification','source_cuts':{},'inputs':{str(Path(__file__)):sha(__file__),str(clang.resolve()):sha(clang)},'rows':[],'full_product_parity':False,'provider_roots':{'dependencies':str(D.resolve()),'qemu':str(Q.resolve())},'cgroup':Path('/proc/self/cgroup').read_text(),'affinity':sorted(os.sched_getaffinity(0))}
 if a.provider_map is not None:
  record['inputs'][str(a.provider_map)]=provider_sha
  record['provider_roots']['by_profile']={k:str(v) for k,v in provider_map.items()}
 record['generated_executable_pins']={}
 for repo in repos:
  manifest=json.loads((a.root/(repo+'-current-manifest.json')).read_text());record['source_cuts'][repo]=manifest['source_id']
  for v in manifest['files']:assert sha(a.root/repo/v['path'])==v['sha256'],v['path']
  for case in cases:
   names=['native_linux_cross_boundary'] if case=='native_boundary' else (['native_step_linux_software','native_step_linux_x86_64','native_linux_cross_boundary'] if case=='native_kernel_step' else [case])
   for name in names:
    p=a.root/repo/'test/0017.runtime'/(name+'.cc');record['inputs'][str(p)]=sha(p)
   if case=='debug_source_dap_expression':
    p=a.root/repo/'test/0018.debugger/dap_source_expression_cases.json';record['inputs'][str(p)]=sha(p)
   if case=='debug_source_dap_reply':
    p=a.root/repo/'tools/debug/dap_adapter.py';record['inputs'][str(p)]=sha(p)
 started=time.monotonic()
 def publish():
  record['wall_seconds']=time.monotonic()-started;(a.out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
 def run(row,stage,argv,log,timeout,env=None):
  began=time.monotonic()
  with log.open('wb') as stream:r=subprocess.run(argv,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout,check=False,env=env)
  row['phases'].append({'phase':stage,'argv':list(map(str,argv)),'returncode':r.returncode,'wall_seconds':time.monotonic()-began,'log':str(log),'log_sha256':sha(log)})
  return r.returncode==0
 for profile,triple,emulator,machine,bits,order,flags in PROFILES:
  if profile not in selected:continue
  D=provider_map.get(profile,a.deps_root)
  sysroot=D/'usr'/triple;gcc=D/'usr/lib/gcc-cross'/triple/'15';qemu=Q/'usr/bin'/('qemu-'+emulator)
  if profile=='x86_64':
   sysroot=Path('/');gcc=Path('/usr/lib/gcc/x86_64-linux-gnu/15')
   if not gcc.exists():gcc=Path('/usr/lib/gcc/x86_64-linux-gnu/14')
  for repo in repos:
   S=a.root/repo
   for case in cases:
    if case=='native_boundary' and profile=='x86_64':continue
    O=a.out/profile/repo/case;O.mkdir(parents=True,exist_ok=False);binary=O/'test.elf'
    row={'profile':profile,'repository':repo,'case':case,'passed':False,'phases':[],'expected_ELF':{'machine':machine,'bits':bits,'byte_order':order}};record['rows'].append(row);publish()
    try:
     if not all(p.exists() for p in (sysroot,gcc,qemu)):raise FileNotFoundError('missing actual sysroot/GCC/QEMU provider: '+str((sysroot,gcc,qemu)))
     name='native_linux_cross_boundary' if case=='native_boundary' or (case=='native_kernel_step' and profile in ('mips32','mips32el')) else ('native_step_linux_x86_64' if case=='native_kernel_step' and profile=='x86_64' else ('native_step_linux_software' if case=='native_kernel_step' else case))
     source=S/'test/0017.runtime'/(name+'.cc');row['source_case']=name
     if case=='native_kernel_step':row['expected_backend']='unavailable/refusal-only' if profile in ('mips32','mips32el') else 'isolated-kernel-fixture, no Wasm issuer qualification'
     linker=D/'usr/bin'/(triple+'-ld') if profile in ('ppc64','ppc32','sparc64') else lld
     record['inputs'][str(qemu.resolve())]=sha(qemu);record['inputs'][str(linker.resolve())]=sha(linker)
     compilation_sysroot=Path('/') if profile=='x86_64' else D
     argv=[str(clang),'--target='+triple,'--sysroot='+str(compilation_sysroot),'--gcc-install-dir='+str(gcc),'-idirafter',str(sysroot/'include'),'-std=c++26','-stdlib=libstdc++','-O1','-g0','-fno-rtti','-fasynchronous-unwind-tables',*flags,'-DUWVM=2','-DUWVM_USE_UWVM_INT','-DUWVM_DISABLE_JIT','-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1','--ld-path='+str(linker),'-pthread','-MD','-MF',str(O/'test.d'),'-I'+str(S/'src'),'-I'+str(S/'third-parties/fast_io/include'),'-I'+str(S/'third-parties/bizwen/include'),'-I'+str(S/'third-parties/boost_unordered/include'),str(source),'-L'+str(sysroot/'lib'),'-L'+str(D/'usr/lib'/triple),'-latomic','-o',str(binary)]
     if profile=='x86_64':
      argv=[v for v in argv if v not in ['-L'+str(sysroot/'lib'),'-L'+str(D/'usr/lib'/triple)]]
     if profile.startswith('mips'):argv.insert(-2,'-Wl,-z,execstack')
     # Native linker dependencies stay on the host side. QEMU below clears
     # inherited LD_LIBRARY_PATH and supplies only this target's SDK libraries.
     native_libdirs=[lld.parent.parent/'lib/x86_64-unknown-linux-gnu',lld.parent.parent/'lib']
     if linker != lld and linker.is_relative_to(D):native_libdirs.insert(0,D/'usr/lib/x86_64-linux-gnu')
     native_libdirs=[p.resolve() for p in native_libdirs if p.is_dir()]
     compile_env=os.environ.copy();inherited=compile_env.get('LD_LIBRARY_PATH','')
     compile_env['LD_LIBRARY_PATH']=':'.join(map(str,native_libdirs))+(':'+inherited if inherited else '')
     row['native_linker_library_path']=compile_env['LD_LIBRARY_PATH']
     for native_libdir in native_libdirs:
      for provider in native_libdir.iterdir():
       if provider.is_file() and '.so' in provider.name:record['inputs'][str(provider.resolve())]=sha(provider)
     if not run(row,'compile-link',argv,O/'compile.log',180,env=compile_env):raise RuntimeError('compile/link failed')
     row['ELF']=elf(binary,machine,bits,order)
     dependencies=shlex.split((O/'test.d').read_text().replace(chr(92)+chr(10),' ').split(':',1)[1]);row['dependency_hashes']={str(Path(p).resolve()):sha(Path(p).resolve()) for p in dependencies}
     record['generated_executable_pins'][str(binary)]=row['ELF']['sha256'];publish()
     libs=':'.join(map(str,([Path('/usr/lib/x86_64-linux-gnu'),Path('/lib/x86_64-linux-gnu'),gcc] if profile=='x86_64' else [D/'usr/lib'/triple,sysroot/'lib',sysroot/'lib64',gcc])));argv=[str(qemu),'-U','LD_LIBRARY_PATH','-E','LD_LIBRARY_PATH='+libs,'-L',str(sysroot),str(binary)]
     target_args=[str(O/'checkpoint.db')] if case=='debug_checkpoint_codec' else []
     if case=='debug_source_dap_expression':
      corpus=json.loads((S/'test/0018.debugger/dap_source_expression_cases.json').read_text())
      target_args=[v['expression'] for v in corpus['positive']]
     if case in ('debug_wasm_memory_mutation_data','debug_wasm_mutation_data') and order=='big':target_args.append('--require-big')
     if case=='debug_checkpoint_codec' and order=='big':target_args.append('--require-big')
     if not run(row,'qemu-execute',argv+target_args,O/'qemu.log',90):raise RuntimeError('actual QEMU target assertions failed')
     row['stdout_sha256']=sha(O/'qemu.log')
     if case=='debug_source_character_literal':
      verifier=S/'test/0017.runtime/run_debug_character_literals.py';record['inputs'][str(verifier)]=sha(verifier)
      spec=importlib.util.spec_from_file_location('character_literal_bridge',verifier);character=importlib.util.module_from_spec(spec);spec.loader.exec_module(character)
      row['character_checks']=character.verify(S,argv,O/'character-probe.log')
      record['inputs'].update(row['character_checks']['inputs'])
     if case=='debug_source_float_range':
      verifier=S/'test/0017.runtime/run_debug_float_range.py';record['inputs'][str(verifier)]=sha(verifier)
      spec=importlib.util.spec_from_file_location('float_range_bridge',verifier);ranges=importlib.util.module_from_spec(spec);spec.loader.exec_module(ranges)
      row['float_range_checks']=ranges.verify(S,argv,O/'float-range-properties')
      record['inputs'].update(row['float_range_checks']['inputs'])
     if case=='debug_source_grouped_postfix':
      verifier=S/'test/0017.runtime/run_debug_grouped_postfix.py';record['inputs'][str(verifier)]=sha(verifier)
      spec=importlib.util.spec_from_file_location('grouped_postfix_bridge',verifier);grouped=importlib.util.module_from_spec(spec);spec.loader.exec_module(grouped)
      row['grouped_postfix_checks']=grouped.verify(S,argv,O/'grouped-postfix-properties')
      record['inputs'].update(row['grouped_postfix_checks']['inputs'])
     if case=='debug_source_builtin_type_specifiers':
      verifier=S/'test/0017.runtime/run_debug_builtin_type_specifiers.py';record['inputs'][str(verifier)]=sha(verifier)
      spec=importlib.util.spec_from_file_location('builtin_type_bridge',verifier);types=importlib.util.module_from_spec(spec);spec.loader.exec_module(types)
      row['builtin_type_checks']=types.verify(S,argv,O/'builtin-type-properties')
      record['inputs'].update(row['builtin_type_checks']['inputs'])
     if case=='debug_source_cast_spacing':
      verifier=S/'test/0017.runtime/run_debug_cast_spacing.py';record['inputs'][str(verifier)]=sha(verifier)
      spec=importlib.util.spec_from_file_location('cast_spacing_bridge',verifier);spacing=importlib.util.module_from_spec(spec);spec.loader.exec_module(spacing)
      row['cast_spacing_checks']=spacing.verify(S,argv,O/'cast-spacing-properties')
      record['inputs'].update(row['cast_spacing_checks']['inputs'])
     if case=='debug_source_dap_expression':
      lines=(O/'qemu.log').read_text().splitlines();assert len(lines)==len(corpus['positive']),'incomplete scalar result rows'
      for index,(line,expected) in enumerate(zip(lines,corpus['positive'])):
       fields=line.split('\t');assert len(fields)==7,line
       actual=list(map(int,fields));assert actual[:2]==[index,expected['status']],(expected,actual)
       for key,position in [('bits',2),('width',3),('unsigned',4),('floating',5),('reads',6)]:
        if key in expected:assert actual[position]==expected[key],(expected,actual,key)
      row['scalar_checks']={'controller_syntax':len(lines),'value_rows':sum('bits' in v for v in corpus['positive']),'expected_refusals':sum(v['status']!=0 for v in corpus['positive']),'zero_resolver_rows':sum(v.get('reads')==0 for v in corpus['positive'])}
     if case=='debug_source_dap_reply':
      spec=importlib.util.spec_from_file_location('dap_formatter_bridge',S/'tools/debug/dap_adapter.py');dap=importlib.util.module_from_spec(spec);spec.loader.exec_module(dap)
      data=(O/'qemu.log').read_bytes();cursor=0;packets={};refusals=0
      while cursor<len(data):
       end=data.index(b'\n',cursor);heading=re.fullmatch(rb'packet ([a-z-]+) ([0-9]+)',data[cursor:end]);assert heading, data[cursor:end]
       size=int(heading[2]);assert 0<size<=dap.MAX_REPLY and end+1+size<=len(data)
       label=heading[1].decode('ascii');assert label not in packets
       text=data[end+1:end+1+size].decode('ascii');cursor=end+1+size;packets[label]=text
       dap.validate_source_evaluation_reply(text,41,1)
       mutations=[text.replace('stop=41','stop=42',1) if text.startswith('source-value ') else text.replace('source-stop 41','source-stop 42',1), text.splitlines()[0]+'\n'+text, text.replace('\n','\x1b\n',1)]
       if text.startswith('source-value '):mutations.extend([text.removesuffix('source-value end\n'), text.replace('source-value end\n','source-type end\n'),text.splitlines()[0]+'\nsource-stop 41\n'+'\n'.join(text.splitlines()[1:])+'\n'])
       for changed in mutations:
        try:dap.validate_source_evaluation_reply(changed,41,1)
        except ValueError:refusals+=1
        else:raise AssertionError(('formatter mutation accepted',label,changed))
      assert set(packets)=={'scalar','object','pointer-display','constant','unavailable-display'}
      assert '\\x1b' in packets['object'] and 'guest:0x100' in packets['pointer-display'] and 'source-origin stop=41 thread=1 ' in packets['constant']
      row['formatter_checks']={'actual_cpp_packets':len(packets),'rejected_mutations':refusals,'copied_display_only':True}
     if case=='debug_checkpoint_codec':row['canonical_checkpoint_sha256']=sha(O/'checkpoint.db')
     if profile=='x86_64':
      native_args=[str(O/'native-checkpoint.db')] if case=='debug_checkpoint_codec' else target_args if case=='debug_source_dap_expression' else []
      if not run(row,'native-reference',[str(binary),*native_args],O/'native.log',90):raise RuntimeError('x86_64 native component reference failed')
      assert (O/'qemu.log').read_bytes()==(O/'native.log').read_bytes(),'native/QEMU control output differs'
      if case=='debug_source_character_literal':
       row['native_character_checks']=character.verify(S,[str(binary)],O/'native-character-probe.log')
       assert row['native_character_checks']['log_sha256']==row['character_checks']['log_sha256'],'native/QEMU character properties differ'
      if case=='debug_checkpoint_codec':assert (O/'checkpoint.db').read_bytes()==(O/'native-checkpoint.db').read_bytes(),'checkpoint native/QEMU canonical bytes differ'
     row['passed']=True
    except Exception as e:
     row['error']=repr(e)
     if isinstance(e,subprocess.TimeoutExpired):publish();raise
    publish();print(json.dumps({k:row.get(k) for k in ('profile','repository','case','passed','error')}),flush=True)
 for p,h in record['inputs'].items():assert sha(p)==h,p
 for row in record['rows']:
  for p,h in row.get('dependency_hashes',{}).items():assert sha(p)==h,p
 for repo in repos:
  m=json.loads((a.root/(repo+'-current-manifest.json')).read_text())
  for v in m['files']:assert sha(a.root/repo/v['path'])==v['sha256']
 controls={(r['repository'],r['case']):r['stdout_sha256'] for r in record['rows'] if r['profile']=='x86_64' and r['passed']}
 for row in record['rows']:
  if row['passed'] and row['case']=='debug_checkpoint_codec':
   control=next((r for r in record['rows'] if r['profile']=='x86_64' and r['repository']==row['repository'] and r['case']==row['case'] and r['passed']),None)
   if control is not None:
    row['canonical_checkpoint_matches_x86_64']=row['canonical_checkpoint_sha256']==control['canonical_checkpoint_sha256'];assert row['canonical_checkpoint_matches_x86_64'],row
  if row['passed'] and row['case'] not in ('native_kernel_step','native_boundary','debug_checkpoint_codec','debug_wasm_memory_mutation_data') and (row['repository'],row['case']) in controls:
   row['same_output_as_x86_64_component']=row['stdout_sha256']==controls[(row['repository'],row['case'])]
   assert row['same_output_as_x86_64_component'],row
 record['passed']=all(r['passed'] for r in record['rows']);record['completed_cases']=len(record['rows']);record['passed_cases']=sum(r['passed'] for r in record['rows']);record['inputs_after_unchanged']=True;publish()
 print('COMPONENT DATA MATRIX',record['passed_cases'],'/',record['completed_cases'],'not full CLI parity',flush=True)
 return 0 if record['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

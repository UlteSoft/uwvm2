from pathlib import Path
import sys,json,csv,shutil,tarfile,hashlib,os,time
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/C25-PENDING-BUNDLE-a1';T=R/'source/uwvm2-candidate-c25-a1';binding=T/'source-binding-c25-a1.json'
def copy(p,d):
 p=Path(p);d=Path(d);assert p.is_file();d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d);assert N.sha(p)==N.sha(d)
with N.lease():
 assert not D.exists();disk=N.BASE/'artifacts/c25-pending-bundle-a1';disk.mkdir(exist_ok=False);D.symlink_to(disk,target_is_directory=True)
 src_binding=json.loads(binding.read_text())
 for f,h in src_binding['files'].items():assert N.sha(T/f)==h,f
 fixed=json.loads((R/'evidence/root-store-nop-fixed-c24-a1.json').read_text());base=json.loads((R/'evidence/length-facts-c25-baseline-a1.json').read_text());witness=R/'source/typed-known-length-ir-witness-c25-a1/witness-a2.json'
 assert fixed['cells']==18 and base['cells']==40 and len(json.loads(witness.read_text())['targets'])==6
 hardware=[]
 for nm in ['root-store-nop-hardware-c24-a1.json','root-store-nop-hardware-c24-dense-a1.json']:
  j=json.loads((R/'evidence'/nm).read_text());hardware.extend(j['rows'])
 assert len(hardware)==15
 for e in hardware:
  sp=Path(e['spec']);s=json.loads(sp.read_text());pd=Path(s['dir']);q=json.loads(Path(e['qualified']).read_text());assert q['hardware_runtime_qualified'] and q['zero_loss'] and N.sha(sp)==e['spec_sha']
  assert (pd/'validation-a3/independent-domain-proof-c24.json').exists()
 for f in src_binding['changed_files']:copy(T/f,D/'candidate'/f)
 copy(binding,D/'candidate/source-binding-c25-a1.json')
 copy(R/'source/uwvm2-candidate-c23-a3/source-binding-c23-a3.json',D/'candidate/source-binding-parent-c23-a3.json')
 for name in ['scoped-c25-known-length-patches-a1.json','build-plan-typed-array-access-known-length-c25-a1.json','build-plan-typed-array-access-inline-static32-c23-a3.json','wasmtime-dynamic-library-binding-c20.json']:
  copy(R/'source'/name,D/'source'/name)
 for name in ['typed-known-length-fixtures-c25-a1','typed-known-length-ir-witness-c25-a1','typed-known-length-c25-a1','root-nop-probe-c24-a1','root-store-nop-codegen-c24-a1','root-store-nop-codegen-c24-dense-a1']:
  for p in (R/'source'/name).rglob('*'):
   if p.is_file():copy(p,D/'source'/p.relative_to(R/'source'))
 for name in ['fixed-root-store-nop-c24-a1','hardware-specs-root-store-nop-c24-a1','hardware-specs-root-store-nop-c24-dense-a1']:
  for p in (R/'source'/name).rglob('*.json'):copy(p,D/'source'/p.relative_to(R/'source'))
 profile_names={Path(json.loads(Path(e['spec']).read_text())['dir']).name for e in hardware}
 profile_names.update(Path(x['dir']).name for x in fixed['rows'])
 profile_names.update(p.name for p in (R/'profiles').glob('fixed-root-store-nop-c24-a1-*'))
 for name in sorted(profile_names):
  pd=R/'profiles'/name
  for p in pd.rglob('*'):
   if not p.is_file():continue
   rel=p.relative_to(pd)
   if rel.parts[0]=='vtune' and not (str(rel)=='vtune/data.0/system-wide.perf' or rel.parts[1]=='config'):continue
   copy(p,D/'profiles'/name/rel)
 prefixes=('fixed-root-store-nop-c24-','root-store-nop-c24-','root-nop-','known-length-','length-facts-c25-','prepare-typed-known-length-c25-','c25-known-length-fragment-','make-typed-known-length-fixtures-c25-')
 exact={'root-store-nop-fixed-c24-a1.json','root-store-nop-hardware-c24-a1.json','root-store-nop-hardware-c24-dense-a1.json','length-facts-c25-baseline-a1.json'}
 for p in (R/'evidence').iterdir():
  if p.is_file() and (p.name.startswith(prefixes) or p.name in exact):copy(p,D/'evidence'/p.name)
 controls={'native.py','disjoint_pmu_c20_fixed_v2.py','disjoint_pmu_read_lock_c20_v1.py','validate_profile_a3.py','root_nop_probe_c24_a1.py','fixed_counter_root_nop_c24_a1.py','profile_worker_root_nop_c24_a1.py'}
 for p in (R/'control').iterdir():
  if p.is_file() and (p.name in controls or ('root_store_nop' in p.name and 'c24' in p.name) or ('known_length' in p.name and 'c25' in p.name)):copy(p,D/'control'/p.name)
 copy(Path('/home/macromodel/Documents/uwvm-gc-perf-20261011-r73/control/enter.sh'),D/'control/enter-cgroup.sh')
 admitted=True;error=None
 try:admission=N.check(build=True)
 except Exception as e:admitted=False;error=str(e)
 status=dict(date_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),candidate='C25 typed numeric array bounds consume existing logical length facts',candidate_source_status='IMPLEMENTED IN PRIVATE SSH COPY; UNCOMPILED; NO CANDIDATE CORRECTNESS OR PERFORMANCE CLAIM',production_status='Qualified C23 retained; this bundle synchronizes candidate source hunks, tests, controls and evidence only',candidate_binding_sha=N.sha(binding),parent_binding_sha=src_binding['parent_binding_sha'],changed_files=src_binding['changed_files'],candidate_has_C24_loop_root_elision=False,qualified_fixed_counter_cells=18,qualified_hardware_profiles=15,qualified_dense_profiles=3,new_Wasm_cases=20,baseline_correctness_cells=40,baseline_measured_invocations=200,cross_codegen_witness_targets=6,cross_codegen_scope='Synthetic LLVM fragment; not actual candidate-generated IR or native/cross-platform performance',memory_gib=int((N.C/'memory.current').read_text())/(1<<30),unique64=N.unique_groups(),swap_max=(N.C/'memory.swap.max').read_text().strip(),memory_events=(N.C/'memory.events').read_text(),build_admitted_now=admitted,build_admission_error=error,unchanged_gates_gib=dict(build=32,test=40,withdraw=48),shared_reclaim_status='New one-shot C24 12GiB reclaim proposal is still pending; NEVER executed in this work',unfinished=['fullC25 3TU native build','candidate new20 + existing379/32auth/80static and actualGC-pressure suites','candidate debug/checkpoint/cache and threeproduction repository builds','candidate performance + hardware qualification before source promotion','lock elision requires explicit ownership/escape proof; NOT implemented by C25'])
 N.save(D/'STATUS.json',status)
 readme="""本轮已在 SSH Linux 唯一的 64 GiB cgroup 中完成 18 组未缩放固定硬件计数和 15 份 VTune 硬件采样；其中 3 份详细微架构采样使用 1 ms 间隔和约 8 秒内部 ROI。全部使用 P-core 2，计时从 JIT 和预热结束后开始，没有 CPU 空闲率门槛。

C23 原代码与“只把三条循环根写入替换成等长 NOP”的受控实验相比，纯写周期比为 0.999636，读写为 1.000044，没有实际收益。该实验只修改本任务暂停的、单线程子进程中的两段无调用 Wasm 热循环，每次调用保留 Wasm 结果断言。它不代表编译后的 C24，也不验证 GC 回收算法。失败或因资源/PMU 中断的尝试保留，均未计入合格结果。

同一 i32 读写 Wasm 用例，原 C23/Wasmtime 的固定核心周期比为 6.126886，退休指令比仅为 1.202354。实际 JIT 汇编中的两个热点锁指令位于 RX 偏移 0x232 和 0x3c2，是内存 xchg。密集采样原 C23 在这两处各有 5892 和 6907 个核心周期事件 IP；Wasmtime 对应循环使用直接堆访问，没有这两次逐对象锁。周期采样可以产生 skid，IP 占比不能当作精确的逐指令耗时；微架构类别比例也不能直接作为可实现的加速比例。

C25 私有候选已修改 numeric array.get / array.set 的 LLVM IR：对象认证和状态成功之后，使用与完整引用 SSA 关联的逻辑长度。构造器长度在 mem2reg 后成为已知值时，优化器可以删除实际长度加载并简化范围检查；未知调用、分支 PHI、空值和局部变量改写沿用已有 fact.valid 协议。锁、对象认证、GC 根和引用写屏障保留。缓存 key 增加 llvm-gc-typed-array-bounds，避免无 source ID 的旧签名缓存误用。此候选独立基于 C23，不包含 C24 循环根发布删除。

20 个新增 Wasm 正确性模块覆盖 i8/i16/i32/f32、旧引用 SSA 与局部变量覆盖、不同长度分支、未知函数返回、别名和 null 替换、变化的动态构造长度。它们在原 C23 和 Wasmtime 上通过 40 组基线检查，每组 5 个已预热调用。这些小用例未触发自动回收，明确允许 collection=0 时 GC phase_pending=2；不能用来宣称回收器或 C25 已通过验证。

独立 LLVM IR 小样在 LLVM23 O3 中验证了：已知长度消除长度加载和范围检查；未知长度保留；认证和锁保留。x86_64 / aarch64 使用本机安装的 LLVM23 llc；ppc64le / mips64el / loongarch64 / sparcv9 使用已安装的 libLLVM22 C API，因为该 LLVM23 工具只编入两个 target。六份汇编属于独立小样，不是实际 UWVM2 候选生成代码，不是跨平台 VM 正确性或性能数据；没有安装或下载新工具。

完整候选构建尚未开始，32 GiB 准入门槛不变；共享文件缓存回收请求未执行。当前三仓库运行源码仍是先前合格的 C23。此目录同步的是待验证候选补丁和证据，禁止据此宣称追平 Wasmtime、宣称 C25 的 Wasm 正确性已验证，或把 C25 直接作为生产改动。

后续首先完成冻结源码的三个 TU 构建，再运行新增 20 项、既有 379 项、鉴权/布局矩阵、真正触发回收的根与压力用例、debug/checkpoint/cache，以及三仓库产品构建。主热点锁的下一步应采用明确的对象逃逸/执行独占证明：例如只对未传出当前激活的新建数值数组考虑锁消除；参数、调用、全局/表/引用字段发布、异常、调试、checkpoint、外部宿主访问均须保守回退。这个锁优化尚未实现，C25 的长度简化不能被当成它的替代验证。
"""
 (D/'README.md').write_text(readme);N.persist(D/'README.md')
 files={str(p.relative_to(D)):N.sha(p) for p in sorted(D.rglob('*')) if p.is_file()};N.save(D/'MANIFEST.json',dict(files=files,status_sha=N.sha(D/'STATUS.json'),scope='Every included file except MANIFEST itself has SHA256; raw native evidence retained, no full VTune database copy needed. Candidate uncompiled status is explicit.'))
 archive=R/'source/C25-PENDING-BUNDLE-a1.tar.gz';assert not archive.exists()
 with tarfile.open(archive,'w:gz') as tar:
  for p in sorted(D.rglob('*')):
   if p.is_file():tar.add(p,arcname=str(p.relative_to(D)))
 assert archive.stat().st_size<64<<20;N.persist(archive);assert N.sha(archive)==N.sha(N.BASE/'recovery/source'/archive.name)
 N.save(R/'source/C25-PENDING-BUNDLE-a1-export-receipt.json',dict(archive_sha=N.sha(archive),bytes=archive.stat().st_size,files=len(files)+1,candidate_status=status['candidate_source_status'],memory_gib=status['memory_gib'],exporter_sha=N.sha(__file__)))
 print('C25_PENDING_EXPORTED',N.sha(archive),archive.stat().st_size,len(files)+1,flush=True)

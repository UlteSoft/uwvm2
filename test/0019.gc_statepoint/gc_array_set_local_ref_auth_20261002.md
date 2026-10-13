# array.set local-reference authentication candidate v1

Source implementation is complete; C++ compilation, native component execution, actual LLVM IR/machine code and VM/performance tests are pending. This source candidate is separate from the immutable 13-overlay collector, R5/R3 runtime packets and S6e measurements. It is not enabled by default.

The exact gate is UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1. Undefined, 0 and 2 keep the original header and LLVM environment text after selecting only this gate. The source-only projection receipt has 12 exact before/projection comparisons and 24 source invariants; it is not a compiler or assembly result.

Only private verifier template plumbing and array_set change in gc_object.h. Every previous verifier caller defaults CaptureLocal=false. The new capture result is a bool, not an object pointer, carrier, ticket or root. value_matches and reference_matches clear its caller-owned slot at entry. finish_local_reference_match sets it only after the ORIGINAL membership query and complete reference/canonical condition succeeded, the result is an ordinary object with owner == this, and expected_owner == this. array_set requests capture only when its ORIGINAL checked destination owner == this, the existing layout says reference storage with no packing, and all original bounds/layout/mutability checks already passed.

For a proved local ordinary source, retain_embedded_reference would query the same source again and immediately return ok for same-owner retention. The candidate omits only that second query. Every foreign destination (including destination/source both owned by one foreign store), foreign source, compact source, null/i31/function/extern/exn case receives false and executes the original retention sequence. There is no speculative membership probe and no extra query on a miss. Type failure still returns invalid_value before retention/write. Null/out-of-bounds/immutable destination precedence is unchanged.

The helper does not inspect an untrusted payload as a pointer. The local node still comes from the original acquire publication chain, with exact token and kind matching. Its pointer is consumed only inside reference verification; the caller receives no source pointer. Existing canonical subtype/ID checks and registry locking remain. The destination object_lock and its acquire/release synchronization remain around the original store.

There is no allocator, VM poll, guest callback, weak-owner promotion or owner release on the successful local proof-to-write path. The bridge's existing managed-page boundary occurs before this path. Real shared GC admission and execution generation lifetime remain the caller's existing requirements through access and cleanup. Neither a bool nor TLS presence authorizes collection/reset/read access. The native component uses real shared/exclusive leases and canonical owner pins; it does not simulate a pause by active_count.

Foreign retention continues to establish both the receiving-module lease and independent destination value_leases where required. Canonically equal types in different stores do not authorize the local shortcut. Exception/native-root retirement and full graph preflight/failed-collection zero-reclaim logic are unchanged. No public C ABI, object layout, bridge symbol, generated-Wasm ABI, validation scan, root frame, TLS state, trap mapping or LLVM status CFG changed. The cache gains only an exact1 key gc-array-set-reference-auth=ordinary-local-same-operation-v1.

## Independent native component, awaiting keeper

test/0019.gc_statepoint/gc_array_set_local_ref_auth.cc constructs real canonical stores and the following native checks:

- Local ordinary struct sources, a declared subtype and a canonical-equivalent distinct type index; local array source accepted by eqref.
- Null nullable/non-nullable, i31 and incompatible function/extern/exn kinds; wrong actual canonical type and kind.
- Never-issued maximum identity, actual native store address, actual native stack-carrier address; none may bypass membership or mutate the good destination.
- Original out-of-bounds and immutable destination precedence; numeric array behavior retains exact high bits.
- Foreign source, both destination/source in one foreign owner, and destination/source in different owners.
- Receiver module lease-owner retirement while the destination's embedded lease actually retains the origin, plus a missing recipient lease-owner failure with unchanged destination.
- Actual reclamation of a once-issued source, fresh publication with a different token, and rejection of the stale source. This proves no stale-token revival, but does not observe the physical allocation address.
- A later invalid root causes reclaimed=0 with both store epochs unchanged; good array readback survives.
- Two real admitted mutators write/read the same reference array under the original object lock. A separate actual execution_domain stop/drain test proves its cleanup waits for the live native generation lease. This is a primitive domain test, not a full VM reset claim.

The main graph creates 15 objects, reclaims one actual unrooted source, and finally reclaims the remaining 14 after all semantic native roots retire. Separate recipient-retirement/failure scopes are outside that main-graph creation counter. Expect two successful main collections and one rejected main collection. Check reset_domain_tested explicitly; a toolchain without UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD cannot claim that portion was exercised.

The inspectable extern-C/noexcept gc_array_auth_fixture_native_set symbol invokes the real store operation. It is a native test entry, not a newly generated ABI. The fixture uses fast_io::io print/perr formatting and no custom numeric parser. No production test callback was inserted between authentication and mutation.

Actual lease-allocation OOM injection, physical native-address reuse proof, real exception payload/census, and VM reset/thread root matrices remain additional qualifications. Do not treat a forged exception kind as a real thrown/retained exception test. No source-only or component result substitutes for these requirements.

## Minimal cold/build plan

The sole Linux keeper runs under the existing actual 64 GiB/swap0 sandbox, with native TLS uniformly selected in every TU and compiler jobs on the approved E-core set. Keep old recipes/raw unchanged. First build the same native fixture with the gate undefined, 0, 1 and 2, without the six experimental macros; record actual source/MD/compiler/DSO/ELF closure before and after. Run correctness first, including ASan for unsafe token/address controls. Add one ON case with compact support explicitly ON to ensure compact-source fallback is really exercised; do not infer that from default legacy objects. No timing/profiling during compilation.

Compile the inspectable native entry to actual O3 LLVM IR and machine object for OFF/ON. The ON ordinary-local success CFG must have one source membership query, full canonical check and one mutation acquire/release; no guest-token inttoptr dereference, weak promotion, poll or allocator may appear on that path. Foreign and compact cases retain the original authentication/lease paths. Presence of the gate or a changed IR checksum alone does not prove this.

After semantics and actual code generation pass, create separate matched fresh all-TU native-TLS products. Baseline/candidate differ only in this gate/source version/cache identity; no mixed old RT/host API source. Use the current official same-byte reference-array allocate/mutate 1M/2M fixtures and exact checksums. Mutation has 2n reference array.set operations and initialization=3072 allocations, attempts/collections/reclaims=0, reason=phase_pending: it qualifies field/lookup cost only. Allocation needs the current exact allocation counts, positive collections/reclaims, roots_requested=1, disabled=0/reason0. A/B iteration includes identical Wasm/setup/readback/root structure.

Unprofiled internal execution and parent wall/user/sys/RSS, whole-guest single-TID cpu_core hardware counts, and VTune sampled/uarch runs remain separate measurement families. Real enabled/running, process/CPU attribution, frequency distribution and SMT evidence are retained; temperature is observation only. Fresh TLS profiling decides relevance. Old S6e map-backed get_thread_state percentages cannot predict this optimization. No speedup, best-VM ranking or default promotion is claimed.

The previous design record GC_ARRAY_REF_AUTH_REUSE_DESIGN_20261002.md remains immutable historical design; this implementation narrows its suggested pointer output to a private same-operation bool.


## 2026-10-07：验证器 ABI 和测试修正

保持原来的非模板 reference_matches/value_matches 签名和完整语义体；只有 array.set 使用单独的 private true 捕获路径，避免把额外 bool 参数和模板实例化传播到短分配等无关操作。捕获只在原始 acquire 成员检查、类型检查成功且普通源/目标/expected owner 都为同一本地 store 后生效，立即省去相同源的第二次保留查找。原始对象锁、generation/admission、foreign lease 和根规则保留，null/i31/compact/foreign/func/extern/exn 走原路径。

开关保持显式启用：-DUWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH=1。未定义、0、2 都关闭；缓存 profile 的启用标记为 ordinary-local-same-operation-v4-original-verifier-abi。没有同步函数对齐探针，也没有将优化默认开启。

修复测试的 canonical 等价声明：不同投影不能作为等价类型，将等价声明放在独立且相同的 singleton rec group。保持线程选择宏跨 fixture 存活，实际执行 generation drain；admission 容量为 2，使第二次进入失败验证关闭状态而不是容量饱和。输出 bool 使用 boolalpha，compact fixture 强制使用真实 compact 后端。

Linux 的 64 GiB / swap0 cgroup 中，两仓库共 28 个原生单元、2,745,696 次检查通过，覆盖默认/OFF/ON/2、ASan/UBSan、实际 compact ON/OFF、foreign/stale/伪造/空引用、真实 native mutator 和 generation drain；六组实际缓存序列化指纹检查通过。最终源码仅清理两处空白行后，还重编译运行了两仓库的 default 和 full-ON ASan/UBSan。两个 native mutator 测试不代表完整并发 Wasm mutator GC 资格。

同算法冻结 CLI 的统一 O3 诊断中，OFF/ON 各通过 370 个 Wasm，用 17 用例×5产品×3进程比较，首调用 JIT/初始化和两轮预热排除。约 50% 的旧混合 O3/O1 写入收益包含编译设置差异；统一 O3 下 typed/eq 写入约改善 14%/17%、dense 混合负载约 11%。这些是该冻结产品的诊断结果，不能推广为所有架构或完整 release 排名。该开关策略最终源码的资格是上面的原生/缓存检查，未声称重新完成全 CLI 闭包构建。保留 std::memcpy 和固定宽度 builtin 拷贝，未做 QEMU 性能推断。

新 gc_array_set_compact_fallback.cc 必须以 -DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=1 编译；分别用本开关 1 和 0 配合既有 full-policy、native TLS 和 ASan/UBSan 配置测试。详细产品 SHA、构建参数、cgroup/CPU 回执和性能范围保存在本任务 R21/R22 元数据。

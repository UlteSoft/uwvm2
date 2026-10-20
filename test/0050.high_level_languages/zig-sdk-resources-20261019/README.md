# Zig resource SDK, native ROS2 scenes and finite source debugging

This is an actual SSH Linux continuation of the [original requirement audit](../REQUIREMENTS_AUDIT_20261019.md). Every compiler, validator, target, Gateway, native peer and debugger ran in the existing cgroup with hard memory.max=68719476736, memory.swap.max=0, boot51c27aa1-d709-4a06-ba16-2ac3003fbcbd and cgroup device29/inode19075. Memory-event counters are unchanged across the admitted runs. No global limits, cache drops or sysctls were changed. Local macOS work was source editing and inert transport only.

## Implemented SDK scope

The ROS checkout adds a caller-storage Zig0.14.1 resource layer. It supplies shared retirement for copied owners, nonblocking reentrancy leases, fixed sessions with reverse cleanup, complete String/Boolean service codecs, full nonzero64-bit request IDs, subscription/service activation, bounded polling, explicit request dispatch over the33-RPC union, separate observer pairing/readiness/submission, output quarantine and required-length sentinel handling.

State and Session must stay at stable addresses and outlive borrowed Owner copies. The layer supports one Guest execution thread and supplies no cross-thread lock. A successful state is never reused. Host checks remain authoritative even if Guest code writes its own fields or calls raw imports directly. Absent-backend operations return95 before importing; paired-lease failure rolls back the first lease. Retirement precedes the effectful close/revoke import and is never replayed.

Request/output limits remain4193786/4194304 bytes. Input and output cannot overlap. Failed or malformed imported output is erased; Result.data is empty. Required size zero is distinguishable from an unchanged all-ones sentinel. Session cleanup retains per-cell outcomes independently of the application error.

The original generated38/43 raw declarations remain unchanged. A fresh15-language source gate also passes891 backend-qualified declarations across22 shared binding rows, plus the separate generated Python call-shape checks. This is source inventory, not full implementation credit. This is a finite API: complete typed Actions, generated vendor record SDKs for Zig, executor integration and upstream rcl/rclcpp/rclpy/rcljava source/API parity remain unfinished.

## Completed producer and ordinary matrices

| Qualification | Actual outcome |
| --- | --- |
| ROS1/ROS2 × Debug/ReleaseSafe/ReleaseFast/ReleaseSmall × resources/checks/unit/original telemetry |32 builds,32 full Wasm validations |
| Reachable ROS imports |24 binary signature checks, selected backend and WASI only |
| Pure private length tests |8 modules intentionally contain no ROS import; not ABI coverage |
| Two frozen interpreter Runtime modes × all32 artifacts, plus resource clock subtraction |112 successful workflows |
| Actual SDK checks |16 executions ×9 groups =144 groups |
| Private length sentinel/quarantine test |16 successful executions |
| Unknown mode, extra argument, oversized fixed argument storage |48 exact exit2 refusals |
| Additional minimal SDK codec probe |2 Debug builds and full validations |

The original publisher remains a distinct lesson. Its denial and compiler outcomes here do not add native publisher credit. SDK checks include real Host13 denial and invalid-handle9/13 refusals, complete wire checks, full-width ID preservation, every RPC selection, alias lifetime/retirement, pair rollback, overlap/bounds and session cleanup. Synthetic handle states are deliberately invalid; these tests grant no native positive authority.

The resource application completes default denial with both clocks, monotonic only, realtime only and neither clock. These Zig paths need no ambient clock grant. This does not remove clock requirements from Go or other runtimes.

All32 main artifacts retain actual .debug_info and .debug_line sections. These sections alone do not qualify source debugging.

## Completed real native ROS2 matrix

Forty positive cases cover five lessons × four Zig optimizations × two frozen UWVM interpreter products. Four additional native controls use Debug Zig on the release Runtime. The final matrix contains44 policy processes and132 scene actors:44 each of Worker, native Gateway and independent native witness. Complete outputs, exact source/product/tool hashes, native mapped code identities, actual held Worker PID binding, ownership/pidfd closure and normal process exits are retained.

| Lesson | Actual native operation | Positive complete value |
| --- | --- | --- |
| Subscribe | Native String publisher → Wasm subscription |bounded native Zig telemetry |
| Trigger | Wasm caller → native Trigger server |success=true; native-zig-ready |
| SetBool | Wasm request=true → native SetBool server |success=true; native-zig-enabled |
| Server | Native Trigger client → Wasm advertise/poll/respond |success=true; bounded zig wasm service |
| Parameter | Wasm typed get → actual declared native String, counted generated GetParameters |bounded native Zig parameter |

The parameter peer disables automatic parameter services and checks the sole requested name value. It qualifies this selected native parameter path only. Subscription checks include exact node/namespace/type, reliable/volatile QoS and a nonzero actual subscriber GID. Fast DDS exposes history/depth as UNKNOWN/0, recorded as unavailable. KeepLast/depth4 is explicit policy; an observed nonunknown value must match it. Services retain actual client/service graph matching, writer GUID/sequence or native future association.

Every positive case includes exact ungranted-alias13 denial, one native operation, complete values, a2s quiet interval and native Gateway node retirement. Subscription endpoint count must become zero. The native service budget remains3000ms; the separate Gateway frame budget remains15000ms. Server sends are driven concurrently with Guest poll/respond credits. No timeout is replayed.

Four controls change actual native subscription text, Trigger text, SetBool success=false with otherwise expected text, and parameter text. Each Guest refuses22 with cleanup-status0 and no success marker. Native control acknowledgements and normal retirement remain checked.

These are Humble x86_64 ROS2 functional outcomes. They do not qualify native ROS1, mixed ROS1/ROS2, all vendor APIs or other architectures.

## Actual finite source DBG and adapter subtraction

A small codec probe calls the real SDK String encoder/decoder and checks complete NUL/UTF-8 preservation. Three actual source-debug sessions on the existing all-adapter LLVM products—uwvm2-ros, uwvm2 and uwvm2-stable—verify the original compiler source breakpoint, scalar code=13 twice at the same stop, a real source step, refusal of the stale stop-id, normal Guest exit and managed debugger retirement.

Eight separate ordinary executions run its two producer artifacts on those three all-adapter products plus the ROS no-adapter LLVM product. Legal Zig Wasm executes when all language debug adapters are disabled. This pure probe contains no ROS import and earns no native-resource source-debug credit.

Earlier debug failures remain visible: the frozen interpreter product refuses debug-jit because LLVM is disabled, and the standard-test-runner module exceeds the selected1GiB then3GiB owned-RSS guard before reaching a prompt. These are failures, not source-debug successes. A controller syntax failure is also retained. The minimal probe isolates a finite codec/source debugging result; it does not relabel the larger module or resource SDK program as fully debuggable.

Keep all15 language-debug options enabled by default. Their xmake switches/macros affect debugger adaptation, not producer-language permission. See [the option guide](../../../docs/debugger/language-options.md). No production xmake option or Host binary code changed in this continuation.

## Sizes and performance boundaries

| Resource Wasm profile | ROS1 bytes | ROS2 bytes |
| --- | ---: | ---: |
| Debug |898273 |898273 |
| ReleaseSafe |826660 |826660 |
| ReleaseFast |98823 |98823 |
| ReleaseSmall |74176 |74176 |

These are actual Wasm file lengths with retained debug metadata, not stripped deployment sizes, native ELF sizes or speed measurements. No new native timing, Wasmtime/WAVM/WasmEdge comparison, VTune hardware Hotspots/Microarchitecture Exploration, QEMU/Windows, cache/CPUID or full-language qualification is claimed. The original full-language requirements remain open.

## Evidence and packaging

The [independent inert reader](independent-readback.json) derives outcomes from exact result files, original raw logs, complete policies/argv, source/tool/product identities, native values/phases and actor closure. It starts no compiler, Runtime, profiler or middleware actor.

Archival C++ source payloads use .cc.source so the root xmake test/**.cc glob cannot create accidental targets. source-payload-map.json records every original path, stored path, byte length and SHA. Restore those original names only in an owned source tree when reproducing a build. Do not treat the evidence directory as a build target.

The packet contains raw final and failed results, complete logs, selected Wasms, frozen source snapshots, the verifier and exact native compiler arguments/dependency hashes. Large native SDK/toolchain/Runtime files remain pinned in the Linux cohort rather than duplicated into Git. Source-only transport preserves the actual Linux path/hash associations. The ROS SDK and detailed English tutorial live in uwvm2-ros; the same acceptance packet is copied to all three repositories without introducing ROS build dependencies into ordinary or stable.

# 实际生成汇编与 PMU 对照

分析使用同一 Wasm 在 C19、C20、Wasmtime 49.0.2 DRC 上执行的 RX 字节，核验执行前后哈希一致，按实际加载地址和原始目标 PMU PC 定位，再分析实际 ELF 原生桥接符号。不是从另一段 C++ 代码推测 JIT，也没有将较短的包装函数误认为真实 getter。

主 f32 用例的 Wasm SHA256 为 `1457757e5e6ce85ae76bf40c8a2fd123b30a42c953576b4e8cb3ae0a173c7e43`。性能 ELF：

| 运行时 | SHA256 |
|---|---|
| C19 | `ac8a9571a4ee8949bc8010d9766130c2d18aee2964f070c933661a335c0ed4ad` |
| C20 | `597c916aef312bcb2d912f310f91fc09b2bb64fcfbeca0afdf88a411c1e527b3` |
| Wasmtime runner | `28d8f798aafea8020b877cb9de888684744fe538475402de034f664725f7ef6d` |
| 实际加载 libwasmtime.so | `e8b92b84cafbd553a8fe1c96ddff1673c3f252849b033b63b11b573ca4e5912e` |

## C19 → C20 的真实变化

C19 的 `array_get32` 桥接包装只有 63 字节，尾跳转到实际 484 字节 getter。真实 getter 仍认证对象、读取实际 owner 的布局/宽度/可变性、检查范围并执行 `xchg` 读取锁。原始 PMU 在其 `0x1034f30` 的 `xchg` 附近记录了 3343 个混合事件采样；包装本身没有 `xchg` 并不代表访问没有锁。

C20 将长度、边界、地址与位加载放进 JIT，并对精确本地类型减少布局查询。主 f32 热循环中实际出现：

```asm
call rbp                         ; 仍需认证对象的 typed borrow bridge
test edx, edx                    ; 返回状态
jne  trap
cmp  QWORD PTR [rax+0x30], r15    ; 原生数组长度与无符号索引比较
jbe  bounds
mov  rcx, QWORD PTR [rax+0x28]    ; 元素存储
movzx edx, BYTE PTR [rax+0x23]    ; packed / legacy 载体
mov  sil, 1
xchg BYTE PTR [rax+0x20], sil     ; acquire，重试分支仍存在
test sil, sil
jne  spin
; packed f32 是 4 字节，legacy carrier 是 16 字节；仍有动态 stride 链
shlx rdx, r15, rdx
mov  ecx, DWORD PTR [rcx+rdx]
mov  BYTE PTR [rax+0x20], 0       ; release
```

上面仅摘录实际循环的关键指令，省略了 stride 选择、校验值比较、根快照和循环控制。真实桥接仍做 token 哈希桶/链认证、kind/owner/type 检查和 foreign 回退。边界失败在拿锁前处理，锁持有期间没有回调或安全点。

因此，减少指令不等于减少串行延迟：固定计数显示该综合用例指令减少 25.9%，周期仅减少 0.6%。C20 detailed Microarch 的 Lock Latency 估计为 40.9，Wasmtime 为 0；这个分层指标不能当作“40.9% CPU 时间花在锁上”，也不能与父子 TMA 指标相加。

## 与 Wasmtime 的真实循环对比

Wasmtime 直接以 GC 堆基址和 32 位对象偏移访问长度，使用编译时的类型布局生成对象整体范围、溢出和元素范围检查，然后 `vmovss` 读取 f32。实际热读循环没有逐次 C++ 桥接，也没有对象 `xchg`。其他位置存在分配等调用，不能把整张代码页的静态调用数当作每次读取的调用数。

新增可变性对照的真实 Wasmtime 热循环包含：

```asm
mov r8d, DWORD PTR [rcx+rdx+0x18] ; 数组长度
mov r9, rdi
and r9d, 0x3ff                   ; 本用例索引
cmp r9d, r8d
jae bounds
; 完整对象尺寸、溢出、GC heap 范围和元素地址检查
vmovss xmm0, DWORD PTR [r10]
vmovd r8d, xmm0
add esi, r8d                     ; 位校验和
```

本机只读的 Wasmtime 源码 HEAD 为 `65abe37529a6bca836efcac1dcb920d5c7890580`。`crates/cranelift/src/func_environ/gc.rs` 中 `translate_array_len()` 和 `translate_array_get()` 的类型化堆访问，与上述机器码方向一致；源码明确指出借用的元素地址不能跨 GC 安全点。本机 HEAD 不是测量的预构建 49.0.2 ELF，两者不能混为同一构建。普通 Rust `Func::call` / `TypedFunc::call` 接收 `AsContextMut`，而 UWVM 还必须保留自己的 native/foreign/debug 并发契约。

## 只改变 mutability 的证据

`read-lock/f32-immutable.wasm` 与 `f32-mutable.wasm` 每个 `_start` 都对 1024 元素做 131072 次相同的带校验和读取，循环内没有 Wasm 写入、调用或 GC 安全点。每个模块在 C20 和 Wasmtime 上使用相同字节。

- C20 immutable 的实际读取循环有一次认证桥接，没有读取锁；mutable 的实际循环多了 `xchg` / release。mutable 的 `0x7c946e209103` 处为 `xchg BYTE PTR [rax+0x20],dil`，原始 Hotspots PC 记录 1441 个混合事件采样。
- Wasmtime 两个实际循环均无逐元素桥接或读取锁。
- 三轮交错计时得到 C20 mutable/immutable = **2.5513**，Wasmtime = **1.0198**。这是数组访问的隔离实验，不是回收器总体基准。
- 额外八份硬件 Hotspots / detailed Microarch 都已零丢样核验。C20 mutable 的 Lock Latency 估计为 50.0，immutable 为 0.0；部分父子指标受多路复用影响明显不一致，故只作为方向证据，不能解释成精确锁耗时或真实竞争比例。

采样 PC 代表采样区间的归因位置，可能有 skid；1441/3343 等记录混合了事件，不能解释为锁等待次数。真实性核验见 [主 JIT 汇编](evidence/actual-typed-array-assembly-mca-c20-v2.json)、[C19 实际 getter](evidence/actual-typed-array-native-callees-c20.json)、[新增可变性汇编](evidence/actual-read-lock-assembly-mca-c20-v1.json)，完整摘出的反汇编与 MCA 输出位于 `assembly/`。

## LLVM-MCA 的用途与限制

LLVM 23 RaptorLake 模型只用于已经执行的指令附近的两个有界直线片段，每片段最多 24 条指令、100 次迭代。它有助于检查动态 stride 的依赖链、加载和执行端口；没有建模完整循环分支/调用、缓存一致性、`xchg` 的实际串行化、缺页和 shared package 争用。因此不能把不同长度片段的 MCA 周期比当作 VM 实测速度比。

后续应合并安全点区间内可证明重复的认证，并用明确的运行权限/并发协议消除不必要的逐元素读取锁。不能把 opaque token 当成对象地址，也不能直接删掉锁或给可能保留 foreign owner 的认证函数添加 `readonly` 属性。

采样归因与模型说明：[Intel Hardware Event-based Sampling](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/hw-event-based-sampling-collection.html)、[Intel Sampling Interval](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2024-1/sampling-interval.html)、[LLVM-MCA 文档](https://www.llvm.org/docs/CommandGuide/llvm-mca.html)。采样 interval 是 SAV 倍率，不是精确的固定毫秒周期。

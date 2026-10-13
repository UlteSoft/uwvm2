# TinyGo map/channel len、cap 调试补齐 — R35，2026-10-07

两个仓库同步新增了有限 TinyGo runtime-backed map 的 `len` 和 channel 的 `len`/`cap`。包含原始 TinyGo 命名类型、nil/空容器、关闭但尚有缓冲数据的 channel，以及 receive-only/send-only channel。返回 guest `int`；C++ IO 和 owned 字符串使用 fast_io。查询沿用已有 source stop/frame/root/cohort 和 guest-pointer 资格，没有新增 host/VM 内存访问权限。

[Go 规范](https://go.dev/ref/spec#Length_and_capacity)规定 map 的 len 为条目数，channel 的 len/cap 分别为排队元素数和缓冲容量；nil map/channel 的相应合法查询为零，map 没有 cap。TinyGo 的 [hashmapLen](https://raw.githubusercontent.com/tinygo-org/tinygo/v0.42.0/src/runtime/hashmap.go) 和 [chanLen/chanCap](https://raw.githubusercontent.com/tinygo-org/tinygo/v0.42.0/src/runtime/chan.go)采用 nil-zero 与对应 header field。实际测试 SDK 的 hashmap 比该 raw tag 文件多 keySlotSize/valueSlotSize；资格以固定 release SDK 和原始 DWARF 为准，没有假定 Git tag 与发布包 ABI 完全相同。

实际 producer 是固定 TinyGo 0.42.0 / Go 1.27.1 / LLVM 22.1.4，tool tree 为 sha256:6f1b439b3d4b8035ded6d57a5de20690b9ea42770019b2516bc853c53fd455ae。原始 Go fixture 使用 canonical `-target wasm-unknown -buildmode default -scheduler none -gc leaking -ldflags -extldflags=-O0 -opt 0`，所有 Wasm features validator 和完整 DWARF verifier 都通过。没有修改 Wasm、DWARF、源码路径或参数有效范围。

普通仓库启动方式：

```sh
uwvm -Rdbg -Rcc jit -Rcm full -Rct 0 -Rllvm-call-stack instruction -Rllvm-cache-path disable --run tinygo-O0.wasm
```

ROS full 产品省去 `-Rcc jit -Rcm full`。在 fixture 的 `GO_COLLECTION_READY` 源码行设置断点，使用实际 participant 与 stop-id：

```text
break-source 0 /absolute/original/debug_source_tinygo_collections.go:<marker-line>
continue
wait
bt <thread>
print <thread> <stop-id> len(box.Numbers)
print-frame <thread> <stop-id> 0 cap(box.Buffered)
ptype <thread> <stop-id> len(box.NilMap)
```

| 原始 fixture 表达式 | 实际值 |
|---|---:|
| len(box.Numbers)、len(box.Named) | 3、2 |
| len(box.Empty)、len(box.NilMap) | 0、0 |
| len(box.Buffered)、cap(box.Buffered) | 2、4 |
| len(box.NamedChan)、cap(box.NamedChan) | 1、5 |
| len(box.Closed)、cap(box.Closed) | 1、3 |
| len(box.EmptyChan)、cap(box.EmptyChan)、len(box.NilChan)、cap(box.NilChan) | 0 |
| len(box.Receive)、cap(box.Send) | 2、4 |
| len(box.Numbers)+cap(box.Buffered) | 7 |

`ptype` 的结果是 `int kind=scalar byte-size=4`。30 个测试表达式使用六个真实编译器生成的参数（mapLen、namedLen、bufferedLen、bufferedCap、closedLen、closedCap），以及 fixture/Go-spec invariants 和推导值；记录逐条区分来源。不会把所有重复比较都计作独立编译器 oracle 或独立功能。每个 live 会话还核对 explicit frame、同停点重复值、ptype、非法表达式、伪造/过期 stop-id、guest 自检、exit 0 和 managed shutdown。

实现识别 exact TinyGo producer、DW_LANG_C99、runtime.hashmap/runtime.channel 指针及完整有限成员布局。实际发布包的新 map header 和 legacy map layout 均有组件测试；legacy layout 没有因组件测试取得真实 producer 资格。channel 仅资格化 wasm/no-scheduler 的零大小 PMutex 布局；其他 scheduler/mutex 布局返回 unavailable。TinyGo DWARF 将这些源码类型擦除成 runtime pointer DIE，适配器不能从被擦除的信息恢复所有 Go 类型身份或别名语义。

非 nil 查询只在已有读取事务中复制一次完整普通 guest header，Wasm32 当前 map/channel 的 extent 分别为 48/36 bytes。它读取 count 或 bufLen/bufCap，并检查 signed guest-int 范围和 len<=cap。不会跟随 buckets、buf、function、queue 指针，不调用 Go/host 函数或取得运行时锁。nil 查询只在已知且经过资格检查的零指针上返回零；未知零填充不能冒充 nil。组件验证 type query 不读值、nil 不读 header、预算在读取前扣除、失败/短读取不伪造值、guest-width 地址溢出、所有成员 metadata fault、foreign producer 和非法 cap(map) 均拒绝。完整控制器仍可能捕获 root guest bytes，不能将组件 reader 计数扩大为整条控制器零读内存。

| 同一最终 cut / 产品的检查 | uwvm2 | uwvm2-ros |
|---|---:|---:|
| 新鲜完整产品 TU + link | 3 + 1 | 3 + 1 |
| 原始 TinyGo validator / 完整 DWARF verify | PASS | PASS |
| 新 collection live，两栈策略，会话 / 比较 | 2 / 60 | 2 / 60 |
| 同 fixture 的旧 R34 产品拒绝，会话 / 表达式 | 2 / 60 | 2 / 60 |
| 原 string/slice 回归，会话 / 比较 | 2 / 18 | 2 / 18 |
| C 普通同名 len/cap 变量，会话 / 比较 | 2 / 16 | 2 / 16 |
| 原 array/pointer-array 回归，会话 / 比较 | 2 / 60 | 2 / 60 |
| C17/C23/C++/Objective-C/Objective-C++，guest32/64，DWARF4/5，两栈策略，会话 / 比较 | 40 / 720 | 40 / 720 |
| 10 分钟重复真实会话 | 65 | 66 |
| 持续值比较 | 1950 | 1980 |
| 持续秒数 | 605.846 | 601.218 |
| collection / array / descriptor UBSan 断言 | 1402 / 450 / 145 | 1402 / 450 / 145 |
| 原 DAP 单元组、正 / 负语法、实际 C++ value rows | 4、595 / 330、595 | 4、595 / 330、595 |
| QEMU profile / 真实目标 ELF | 16 / 48 | 16 / 48 |

短会话合计 96 个、1748 次值比较；持续会话另有 131 个、3930 次值比较。旧产品 4 个会话拒绝 120 个表达式。32 个 QEMU repository/profile job 执行 96 个真实目标 ELF，合计 63904 条组件断言。重复比较不是独立功能数。

两个冻结 source ID 为 sha256:236c37b428d0a9f17c158ed52b8097dd9a449693ebd6a9890623e1b4676175cc 和 sha256:7ab8b0026feec0fcf33f4586335e2f56fb2c3911bdece88b884dbbf62461bc20；最终 VM SHA-256 分别为 731c33be5994826e48319bff699cf569eab8e8262bfb406f74878e3d9fc0ada3 和 23315624053e752e67a397fb48881044640fbe61e5e5149291d1e3fd539e0af0。最终 cut 的 52 个 guard job 通过；本轮全部准备/最终阶段 56 个 guard job 通过，另保留 5 个失败 receipt 和启动前失败记录。全部已启动 owned roots 回收，descendant PIDFD 退休，memory.events 保持零。

16 个 QEMU Linux profile 为 x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel。每个仓库/profile 执行 collection、array、descriptor 三个真实目标 ELF；机器类型、位数、端序和 GNU_STACK flags 全部核对。MIPS 沿用隔离 DATA fixture 的 execstack 选项。这只证明这些生产查询 DATA 组件的语义一致，完整跨架构 VM/JIT、语言运行时、单步/寄存器/ASM 调试体验仍需分别验证。

Linux 本轮再次重启后复用了原 64 GiB / swap=0 / 原 CPU 集合 cgroup，重新挂载原 16 GiB ext4 image，FSID 12191019222208619510 未变。所有编译、功能 Python 和目标程序均经 birth/PIDFD supervisor 在该 cgroup 中执行。full-build/native/producer 保留 16 GiB owned RSS / 2 GiB output；CLI/QEMU 各自保留 1 GiB RSS / 64 MiB output、8 MiB 单文件、1 MiB root log；原 9288400896-byte arena floor、25 GiB host floor、inode、OOM 和 exec/cwd 检查都保留。环境管理和证据归档只做管理性操作。失败编译/构建入口尝试的 receipts/logs 保留，最终成功 cut 独立验证。

原始 producer/metadata 与最终产品、6 个拥有路径和独立 helper pins 都核对。审阅记录所列的每仓库并发源码差异保留；没有将冻结 cut 的结果冒充整个最新工作区已测试。历史 LLVM23 ROS.11 developer provider 被单独固定，最新 ROS 完整 LLVM provider parity 尚未验证。ASM 仍仅限 Wasm 产生的上下文，本轮没有修改 ASM、checkpoint 或 WASIp1 接口。

map entry indexing/iteration、其他 scheduler/runtime 布局、所有 producer/别名、builtin shadowing、完整 Go grammar/优先级/untyped constants、gc Go 真实语言运行时、goroutines、Rust `.len()`、完整 Go DAP broker/IDE 和所有语言完整 native 体验仍未完成。TinyGo O1 的 upstream runtime inline ranges 仍需严格 DWARF 验证，未放宽 verifier。能力清单 `go_len_cap` 保持 partial；其他 87 个 feature 和历史 followup 记录保留。

机器记录：[debug_go_collections_qualification_20261007.json](debug_go_collections_qualification_20261007.json)。公共入口为 `debug_source_go_collections.cc`、`run_debug_source_tinygo_collections_cli.py`、`run_debug_tinygo_collections_soak.py`、`run_debug_go_collections_qemu.py`，必须由 Linux cgroup supervisor 驱动。

完整证据主归档：/home/macromodel/Documents/uwvm3-implementation/retained-go-collections-r35-final/go-collections-r35-evidence.tar.xz，117157428 bytes，SHA-256 f8df629445df01ed8134484d83b5460f29c299d37496017f088592e7d0053250，全部 9204 个成员逐一核验并 fsync。包含最终冻结源码、两份完整 VM、六个对象、成功/失败日志、guards 和已验证的跨架构嵌套归档。每个完成 QEMU job 的原 ELF/日志在独立 verified archive 中保留，完成并退休 PIDFD 后才移除 arena 中的重复输出；侧车保留同 SHA 的结果记录。SDK/QEMU 大目录只按 exact external pins 保留，没有安装到系统。

准备阶段另归档到 /home/macromodel/Documents/uwvm3-implementation/retained-go-collections-r35-preparation-a1/r35-preparation-evidence.tar.xz，14814056 bytes，SHA-256 b175793acfd1893e81ee43be94df44ad3337c7ea23b97f3a71fda8a5da0e761a；15433 个成员均核验。只退休两份旧 cut 已验证的重复源码路径，统计为 logical file bytes，未冒充实际物理释放量。六个 raw object 的 host 重复副本在主归档核验后退休，可从主归档恢复；最终 VM、冻结源码及原始 receipts/logs 保留。历史 R33/R34 归档没有改写，R34 已验证的旧产品迁移/重复 QEMU 输出退休另有 migration proof。

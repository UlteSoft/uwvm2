最新原始类型谓词的 Linux 跨架构验证与公共测试入口修复（R49）

补齐 R48 未在最新版本执行的14个架构。SDK并未全部缺失：原单个provider根只有x86_64/ppc64，但早期留存的独立sysroot仍可使用。本轮核验原恢复证明及实际文件SHA，复用6个原provider，不下载、安装或复制SDK。

两仓同步修改公共 `run_debug_linux_qemu_components.py`：新增 `--provider-map`，让每个选定profile显式选择自己的SDK；登记 `debug_source_language_predicate`；编译成功后、实际执行前记录ELF哈希。配置必须为最多64KiB的JSON对象，完整列出所选profile，拒绝重复键、未知/缺失键、相对或不存在的目录等错误，发生错误时不创建输出目录或启动子进程。配置只选择测试提供者，不授予调试权限，也不能替代原cgroup监督。

实际测试发现公共入口缺少LLVM linker宿主动态库路径，导致 `libc++.so.1` 无法加载。现在显式提供SDK `lib/x86_64-unknown-linux-gnu` 和 `lib`；GNU cross-linker另使用对应provider的宿主库目录，并记录库文件哈希。LLD按实际选择的linker区分，避免SDK map根为 `/` 时误用宿主系统库目录。QEMU仍清除继承的LD_LIBRARY_PATH，只使用目标架构SDK库路径。

配置示例（仅作为原监督器内的命令参数，仍须通过原cgroup/PIDFD/资源预检）：

```json
{"aarch64":"/verified/aarch64-sdk","ppc64":"/verified/ppc64-sdk"}
```

指定 `--profiles aarch64,ppc64 --provider-map /path/providers.json --cases debug_source_language_predicate`。未指定map时保留 `--deps-root` 路径选择。配置必须包含所选profile的每个键且没有其他键；目录存在仅是配置预检，实际工具、SDK、依赖和运行结果仍须逐项验证。

两仓各11项新增配置单测通过。最新冻结公共入口实际执行16个Linux QEMU profile、两仓共32个目标，每个目标通过721385条primitive谓词检查，总计23084320条。每次组件同时覆盖guest32/64两种地址宽度，三类语言比较、bool类别、声明预检、短路、整数取反、Rust/Go优先级及同类浮点边界；测试用有限DATA解析器回调，并非真实各语言调试停点。

| Linux profile | uwvm2 | uwvm2-ros |
|---|---|---|
| x86_64 | PASS | PASS |
| aarch64 | PASS | PASS |
| i686 | PASS | PASS |
| riscv64 | PASS | PASS |
| ppc64 | PASS | PASS |
| ppc64le | PASS | PASS |
| ppc32 | PASS | PASS |
| mips64 | PASS | PASS |
| mips64el | PASS | PASS |
| mips32 | PASS | PASS |
| mips32el | PASS | PASS |
| sparc64 | PASS | PASS |
| loongarch64 | PASS | PASS |
| s390x | PASS | PASS |
| armhf | PASS | PASS |
| armel | PASS | PASS |

实际ELF machine、位宽、大小端均验证，32份目标输出与R48已固定的原生x86_64参考逐字节一致；x86_64另有两次直接原生执行对照。固定生产源码仍为R48两仓source cut，本轮没有修改C++调试器核心或IO。公共脚本/单测另冻结在R49；不把当前整个脏工作区或latest ROS LLVM provider列为已测。

公共入口总墙钟时间 585.892 秒；最终监督任务 607.882 秒（包含哈希、编译及监督，不是性能基准）。最高自有RSS 612585472 bytes，自有输出峰值 29518863 bytes。所有配置单测、编译、链接、原生及QEMU执行都在原SSH Linux64GiB/swap0 cgroup中，原birth/PIDFD/CPU/身份/资源及回收检查保留；OOM计数未增加，自有进程均退役/reap，keeper和peer保留。

仍保持6GiB共享内存余量准入、1GiB自有RSS、64MiB自有输出、8MiB单文件、1MiB日志、arena free floor9288400896 bytes及host25GiB保留。未降低full产品/生产者工作原门槛。

失败记录原样留存：a1漏传CPU列表，公共预检在编译前拒绝；a2 linker缺少动态库路径；a3遗漏SDK triple库子目录；a4校正 `/` provider根下的linker分类。a2/a3/a4经各自原监督器受控取消并回收自有子树；只把最终a5完整成功版本计入32次执行。失败/取消版本不计通过，不归类为VM故障。

完整证据包 12120608 bytes / 1157 members，SHA256 `d19154acf9490416ee698819b4679a2ce0957bfdf2244311a6acb4d05bfed191`。源码、输入清单、实际依赖、目标ELF、原始输出、单测和失败/成功监督记录均留存；逐成员流式SHA和fsync已验证，不复制工具链或大产品。Linux留存目录 `/home/macromodel/Documents/uwvm3-implementation/retained-predicate-cross-r49-final`。

本轮补齐的是最新primitive DATA组件的跨架构覆盖。完整QEMU VM/JIT/DAP、真实Zig producer、标准Go完整运行时/named bool、AssemblyScript完整表达式、native GDB/IDE GUI体验仍未完成；16个profile通过不表示完整调试体验或性能一致。原语言能力统计状态保持，ASM仍限Wasm生成guest上下文，禁止调试VM自身；ROS模式限制没有修改。

机器证据：[qualification](debug_native_predicate_cross_qualification_20261008.json)。上一轮真实Rust/TinyGo DAP及限制见[R48报告](README.debug-native-predicate-r48-20261008.md)。

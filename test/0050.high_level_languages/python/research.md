# CPython → WASI：官方路线与桥接审阅

2026-10-18，仅静态调研。资料限 CPython / Python 官方文档及 WebAssembly 官方工具链源；没有安装、下载源码包、构建、执行 Python/Wasm/DBG 或 Linux 命令。producer/fixture 由另一 agent 实现，ROS SDK 与 examples 由 ROS 任务维护。本文件给出设计依据和待验证条件，不是运行通过报告。

## 固定版本与实际目标

建议固定 **CPython 3.14.8 + WASI SDK 24.0**，不跟随 main。CPython 的官方 XZ 源码包约 23 MB，发布页 SHA-256 为 `c2215904f02b175596dc49351585104f4bc20341e1c47378b26a2c274360ce73`；producer 应在 SSH Linux 下载后另存实际 archive hash、来源、解包树与补丁 hash。[官方发布页](https://www.python.org/downloads/release/python-3148/)

若 python.org 网络不可达，官方 GitHub 的 v3.14.8 tag 指向 commit `8e6e75d9102e39bed2a2b279203a396741180f12`。可将该完整 commit 的 checkout/autoarchive 作为单独固定的官方源码来源。GitHub release 的两个 assets 是自动生成的 source ZIP/tar.gz，不是另行上传的 python.org XZ 发布包；不能复用上面的 archive hash。实际下载者需另记 autoarchive/树的 SHA 与生成 configure 等文件的存在性。[release/tag](https://github.com/python/cpython/releases/tag/v3.14.8)、[完整 commit](https://github.com/python/cpython/commit/8e6e75d9102e39bed2a2b279203a396741180f12)、[assets 列表](https://github.com/python/cpython/releases/expanded_assets/v3.14.8)

该 tag 的构建入口为 `Tools/wasm/wasi`，默认 target 为 `wasm32-wasip1`，源码声明的 supported SDK major 为 24。SDK 25+ 的 VERSION 不匹配会警告；这不是“所有 SDK ≥24 都已验证”的承诺。3.15/main 的入口已迁到 `Platforms/WASI`，不能把新路径直接套到此固定版本。SDK archive/clang/sysroot 的实际 SHA 仍由 producer 在 Linux receipt 中补齐，本调研没有虚构其校验值。[固定构建脚本](https://github.com/python/cpython/blob/v3.14.8/Tools/wasm/wasi/__main__.py#L33)、[官方构建指南](https://devguide.python.org/getting-started/setup-building/#wasi)、[SDK 24 release](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-24)

这是将 **CPython 解释器及其 C runtime 编译为 core Wasm**，在 guest 线性内存中执行 Python bytecode。不是把任意 `.py` 直接 AOT 成独立 Wasm 指令，也不是 Emscripten/浏览器 loader、WASI component/P2、memory64 或 WasmGC 路线。32 位 target 的指针是 guest offset；不能当作宿主指针。SDK 的 libc 提供 POSIX→WASI 适配。[固定 SDK 说明](https://github.com/WebAssembly/wasi-sdk/blob/wasi-sdk-24/README.md)

最终产物必须独立列出 memory 定义/导入、地址宽度、entry/export 和完整 imports；预期标准 command 依赖 `wasi_snapshot_preview1`，但不能根据 target 名称提前宣称某个尚未生成的模块“只有 P1 imports”。stdio、argv、environ、clock、random、文件相关 imports 以实际链接结果为准；缺失 host import 要报告为具体边界，不能 fake 成成功。

## 标准 CLI / stdlib 基准先行

官方流程需要同版本 native build Python 和 WASI host Python 两次构建，系统 Python ≥3.11 用来驱动脚本。标准路线应先保留：固定源码、SDK24、release 与带 C DWARF 的 debug 产物、独立 stdlib/bundle、明确 guest 目录映射。release 的性能与 `--with-pydebug` 的解释器一致性检查分开。[官方构建指南](https://devguide.python.org/getting-started/setup-building/#wasi)

脚本提供 configure/build 的分步入口。默认生成的 `python.sh` 把 checkout 映射到 guest `/`，用 `/Lib` 加载 stdlib，并显式设置 sysconfig 的 PYTHONPATH 与 argv0。因此裸 `python.wasm`、`-c` 或 stdin **不能自动证明不需要 stdlib 文件**；它们也必须先完成解释器初始化。[固定 host-runner 实现](https://github.com/python/cpython/blob/v3.14.8/Tools/wasm/wasi/__main__.py#L280)

可选独立 bundle 应保留文件 manifest、guest 路径、所需模块和 SHA。ZIP 只能承载 Python 模块等资源，不能据此加载平台 C 扩展；压缩形式是否需要额外 zlib 也须核对实际模块构建。最小基准可先使用普通目录 bundle，避免未经验证的打包变化阻塞语义测试。[官方 zipimport](https://docs.python.org/3.14/library/zipimport.html)

资源注意：固定脚本的 make 步骤按 CPU 数决定 jobs；宿主 Python 没有 `os.process_cpu_count` 时会退到 `os.cpu_count`，可能不遵守 taskset 对可用 CPU 的限制。建议使用官方 configure 分步后，在两个隔离 build 目录显式 `make -j2`，并记录命令。未测量之前不承诺编译 RSS、磁盘或耗时；先复用已有 SDK24/cache、清点持久 arena 空间，预留数 GiB scratch 仅属计划预算。所有实际步骤仍须原 SSH Linux 64 GiB cgroup、由协调者释放窗口。[jobs 计算与 make](https://github.com/python/cpython/blob/v3.14.8/Tools/wasm/wasi/__main__.py#L7)

## 无 preopen 的有限冻结候选

结论是**源码支持这个候选设计，但尚未证明这组模块在 WASI 无 preopen 完成启动**。标准 CLI + 显式 bundle 始终保留为独立基准。

默认 frozen stdlib 已含 `abc/codecs/io`，以及 `os/stat/genericpath/posixpath/_collections_abc` 等；默认没有冻结 encodings，源码注释明确将它排除。codec 初始化又必需导入 `encodings`，因此仅使用 `-S` / isolated config / 纯内建脚本仍不能绕过缺失 encodings。[默认冻结清单](https://github.com/python/cpython/blob/v3.14.8/Tools/build/freeze_modules.py#L46)、[实际 frozen 表](https://github.com/python/cpython/blob/v3.14.8/Python/frozen.c#L68)、[codec 启动](https://github.com/python/cpython/blob/v3.14.8/Python/codecs.c#L1591)

有限 UTF-8 fixture 可额外冻结原版 `encodings/__init__.py`、`encodings/aliases.py`、`encodings/utf_8.py` 与 fixture。前者依赖已有 `codecs/sys` 和 aliases，UTF-8 codec 走已有 C codecs。这只覆盖明确固定 UTF-8 的路径，不代表 aliases 表里提到的所有其它编码实现也被打包。[encodings 包](https://github.com/python/cpython/blob/v3.14.8/Lib/encodings/__init__.py#L29)、[UTF-8 codec](https://github.com/python/cpython/blob/v3.14.8/Lib/encodings/utf_8.py)

可在 `Py_InitializeFromConfig` 前设置 `PyImport_FrozenModules` 指向持久、零 sentinel 终止的私有表。3.14.8 的 `_frozen` 有 `name/code/size/is_package` 四字段：包名应为 `encodings` 且 is_package=1；两个子模块和 fixture 为普通模块。不能沿用旧版“负 size 表示 package”的布局。使用同一 CPython tag 的 build Python / freeze 工具生成 marshal 数据，保留 `compile` 的真实 source filename 与内容 hash；不能用本机不同 Python 的 marshal/pyc。自定义表先于普通 frozen stdlib 查找，bootstrap 表仍独立。[实际 header](https://github.com/python/cpython/blob/v3.14.8/Include/cpython/import.h#L12)、[查找顺序](https://github.com/python/cpython/blob/v3.14.8/Python/import.c#L2775)、[官方导入 API](https://docs.python.org/3.14/c-api/import.html#c.PyImport_FrozenModules)

嵌入 launcher 必须明确配置，而非只调用 isolated initializer：

| 设置 | 审阅要点 |
| --- | --- |
| `PyConfig_InitIsolatedConfig` | 隔离用户路径和 Python 启动环境配置；本身不会把默认 site_import=1 变成 0 |
| `site_import=0`、`use_environment=0` | 关闭 site 与环境对启动配置的影响；后者不等于清空 WASI guest environment |
| `use_frozen_modules=1` | 必须明确设置；Py_DEBUG 默认关闭普通 frozen stdlib，自定义表能查到不代表 codecs/os 等既有表也会启用 |
| `module_search_paths_set=1` | 明确有限/空列表，避免自动加入未准入路径；还要记录 prefix/program_name 等最终路径配置与实际 sys.path |
| UTF-8 preconfig / filesystem / stdio | 在会触发 preinitialize 的 argv setter 之前确定 UTF-8；防止启动另找 ascii/locale codec；不把 PYTHON* 环境值当测试结果 |
| `parse_argv=0` + `PyConfig_SetBytesArgv` | 嵌入 fixture 的参数不被再次解释为 Python options；setter 会预初始化并按配置解码字节。明确 argv0；Python 规则不等于 Java 自动去掉 argv0 |
| 每次 API status 与 cleanup | 检查错误、清理 config、正常 finalize；初始化成功与 fixture 成功分别记录 |

上述字段、默认值和 setter 的依据为[固定 initconfig 实现](https://github.com/python/cpython/blob/v3.14.8/Python/initconfig.c#L1081)、[公开配置 header](https://github.com/python/cpython/blob/v3.14.8/Include/cpython/initconfig.h)、[初始化文档](https://docs.python.org/3.14/c-api/init_config.html)。

`sys/posix/time/_io/_codecs` 等由核心或 bootstrap 静态模块提供；必须核对实际 `sys.builtin_module_names` 和链接结果。不能把拥有 `.py` 的模块名等同于 C 扩展可用。特别是 `_ctypes/libffi`、任意宿主 shared library、原生 ROS Python wheels 不在本候选保证范围。[静态 bootstrap modules](https://github.com/python/cpython/blob/v3.14.8/Modules/Setup.bootstrap.in#L3)

无 preopen proof 至少包括：guest 无目录映射，初始化/最终 sys.path 记录；实际 startup、Unicode argv/env、bytes stdin/out、正常退出；缺失冻结依赖或试图访问未准入 stdlib 时保留原失败。只看输出一个常量不能证明完整封装。

## Python DBG 与 native/WasmDBG 分层

给 CPython C runtime 编译 DWARF，可以调试解释器 C 源/对应 Wasm 指令与 native JIT；它不会自动为被解释的 `.py` 生成等价 DWARF 变量/源码断点。`_PyEval_EvalFrameDefault` 等热点或 raw Wasm locals 也不是 Python 函数/变量证明。

可行的有限 Python bridge 是 `sys.settrace` + 真实 frame：按 code filename/function 和 `f_lineno` 过滤，在 line event 暂停，通过 `f_locals` 读实际 Python 值，再由 stdio 命令放行单步/继续。line event 发生在该行执行前；必须选择两个真实 source stops，在 native CPython oracle 与 WASI guest 上核对更新前/后的值、行号、异常和退出。保留原 Python 源码/filename，不伪造 Wasm debug section。trace 是逐线程能力，有限单线程 fixture 不能扩张成多线程/任意对象调试支持。[tracing 语义](https://github.com/python/cpython/blob/v3.14.8/Doc/library/sys.rst#L1601)、[frame 属性](https://github.com/python/cpython/blob/v3.14.8/Doc/reference/datamodel.rst#L1527)

这是 guest 内 Python 级 debug protocol；它与 UWVM CLI 的 WasmDBG/source-DWARF 流程应分开计数。如果 ROS/VM 尚无接入桥，只能报告该 Python 协议通过，不能声称 UWVM 原生命令已能 `print` Python locals。trace 不进入性能 artifact，性能测量前明确关闭。

标准 `pdb` 可另作完整 bundle 的后续方向，但 3.14.8 顶层依赖 `bdb/cmd/dis/inspect/socket/asyncio/typing/linecache` 等，不能假设最小冻结 encodings 就足够，也不能把 Python 3.14 的宿主进程 remote attach 直接当成 WASI guest 支持。[固定 pdb 依赖](https://github.com/python/cpython/blob/v3.14.8/Lib/pdb.py#L61)

## stdio / args / env / bytes 与 ROS ABI v1

标准 CLI 的 `-c` 令 `sys.argv[0]` 为 `-c`；嵌入模式需自己定义 argv0 后核对 payload。空 argv 元素、非 BMP UTF-8 和 embedded `=` 应与已有语言 fixture 对齐。Python argv 的字节→字符串转换取决于 filesystem encoding/error policy；以实际 Unicode codepoints 与可回转 bytes 断言，不按打印外观判断。[sys.argv 官方规则](https://github.com/python/cpython/blob/v3.14.8/Doc/library/sys.rst#L68)

`os.environ` 来自进程环境快照，`os.getenv` 查该 mapping；不是 TeaVM 的 null stub。isolated `use_environment=0` 只忽略 Python 启动配置变量，显式传给 WASI 的业务环境仍应由 os.environ 读取。测试空值、缺失、UTF-8、多个 `=`，并在首次 import 前设置，不能假定 host 后续变更会自动刷新。[os 源码](https://github.com/python/cpython/blob/v3.14.8/Lib/os.py#L737)、[环境快照规则](https://docs.python.org/3.14/library/os.html#os.environ)

ROS 的权威合同是本仓 [abi-v1.json](../../../sdk/abi/abi-v1.json) 与 [ROS1 C header](../../../sdk/ros1/abi.h)、[ROS2 C header](../../../sdk/ros2/abi.h)。这里的 memory index 是 i32，**offset、length、handle 是 i64**；不是 i32 offset/length ABI。`capability_open(i32 memory, i64 alias_offset, i64 alias_length, i64 handle_out_offset) -> i32 errno` 成功写八字节 LE handle，默认无授权应返回 deny=13。WASI P1 自身的 wasm32 I/O 指针参数仍是 i32，二者不能混淆。

ROS 可复用两种边界，均须实际 SDK 合同与端到端测试后再称适配成功：

- **纯 Python bytes 协议：** `sys.stdin.buffer` / `sys.stdout.buffer` 传二进制 payload，协议明示版本、长度、端序与错误。`int.to_bytes/from_bytes` 可避免为有限 fixture 增加 struct/json 依赖。测试 `b''`、内嵌 NUL、0x80/0xff、UTF-8 bytes、分段读写/EOF/截断；stdout 数据与 stderr 日志/DBG 控制分离。stdin/out 路径证明数据桥，不自动证明 ROS 函数调用 ABI。
- **静态 C extension 包装现有 ROS ABI：** 使用上面的真实数值签名；Python 侧仅接受 bytes/明确 buffer。`PyBytes_AsStringAndSize` 提供指针与长度，不能用 strlen，不能传 PyObject* 或宿主地址。Wasm32 指针先经 `(uint64_t)(uintptr_t)` 无符号零扩展再传 i64；不可先转成有符号 int32 再扩展。length 还受有符号 Py_ssize_t 范围约束；校验负数、溢出、上限与返回码。八字节输出 slot 只在 errno=0 后按 LE 解码；handle/ticket 保留全部 64 bit。同步调用期间保留对象引用；异步/retain 场景复制到 SDK 自有 buffer 并明确释放。memory.grow 后重新解析线性内存基址，不保留失效宿主映射。实际 namespace、ownership、回调/GIL 和错误映射由 ROS SDK 合同决定，本文件不创造另一套 SDK。[官方 bytes C API](https://docs.python.org/3.14/c-api/bytes.html)

这两条都不需要 ctypes。ABI 数据范围、buffer ownership 与输出 oracle 应可供 Java/C#/MoonBit SDK 复用，但不能仅凭 Wasm32 target 或相同标量形状宣称跨语言 ABI 已一致。完整 81 imports 由 ROS SDK 任务实现；本任务只准备固定 producer 与有限 generic bridge 验证。

## producer 交付应审阅的证据

1. 原始源码 archive/tag、SDK24 与工具链/库 hash，隔离构建命令、jobs、cgroup/boot/CPU/内存事件；保留原 baseline 与 optional launcher，不覆写已有语言产物。
2. 实际 Wasm SHA/imports/memory/exports、stdlib manifest；bundle 与 frozen 方案分列，初始化失败保留。native build Python 与 guest 使用同版 sources，冻结字节由匹配工具生成。
3. native CPython 与 Wasmtime 完整 stdout/exit oracle；三仓普通结果；Python 级 DBG 和 WasmDBG 各自真实协议、source/value 断言、正常退出/owned retirement。
4. clock 若采用 `time.perf_counter_ns`，先记录 guest `get_clock_info` 并核实实际 monotonic 实现与 P1 clock import；不要把 process_time/thread_time 当作 WASI 单调墙钟。timed/gated/debug 产物独立 SHA，不能复用旧未修改模块的身份。[WASI time 边界](https://github.com/python/cpython/blob/v3.14.8/Modules/timemodule.c#L1249)
5. ROS 的 stdio bytes 与直接 C ABI 分列成功范围。正式 VTune/engine benchmark 待所有正确性与桥接资格完成后由协调者独占运行；本调研无性能结论。

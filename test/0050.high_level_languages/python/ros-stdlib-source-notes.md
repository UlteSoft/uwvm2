# CPython 3.14.8 WASI：ROS Python 标准库边界（源码审阅）

2026-10-18，仅审阅 CPython 官方文档和 `v3.14.8` 源码；没有安装、构建或运行测试。固定源码提交为 `8e6e75d9102e39bed2a2b279203a396741180f12`，生产者路线与 SDK 约束见 [research.md](research.md)。本文描述候选依赖和兼容边界，不是完整 rclpy、其执行器或 native DDS 已运行的证明。后续所有实际测试仍须在 SSH Linux 的原有 64 GiB cgroup 内完成。

官方说明区分“模块可导入”与“操作可工作”：WASI 只提供部分 POSIX 能力，进程 API 可能缺失或始终报错，socket 受限。不能由宿主 Linux 的能力推断 guest 拥有相同能力。[WebAssembly 平台说明](https://docs.python.org/3.14/library/intro.html#webassembly-platforms)

**线程、进程和网络的合同边界**

| 接口 | 官方文档／固定源码证据 | 本轮 ROS 迁移应声明的范围 |
| --- | --- | --- |
| `_thread` | 不是简单的“完全不存在”。无真实 pthread 的 WASI 配置选取 `posix_threads=stub`，定义 `HAVE_PTHREAD_STUBS`；`thread.c` 选择 stub 实现。 | 导入、线程标识、某些锁对象可供单线程标准库使用；这些结果不能证明 guest 可创建线程。 |
| `threading` | 官方明确标注 **not WASI**。stub 中 `pthread_create` 返回 `EAGAIN`，mutex/condition 的多个函数直接返回 0。 | 不承诺 `Thread.start()`、原生互斥、条件等待、并行 callback 或 `ThreadPoolExecutor`。实测时需区分 import、构造与真正启动。 |
| `multiprocessing` | 官方明确标注 **not WASI**。 | 不承诺 Process、Pool、进程共享内存或跨进程执行器。 |
| `subprocess` | 官方明确标注 **not WASI**；平台说明特别指出其可能可导入但不能工作。 | 不承诺 `Popen`、shell、fork/exec、外部 ROS CLI；不以替换为成功空操作的 stub 掩盖限制。 |
| `socket` | P1 只允许使用已有文件描述符的 socket；具体构建还影响模块与方法的存在。 | 不承诺 guest 自行创建／连接 TCP/UDP、DNS、DDS discovery。已授权的 host ROS 传输不等于开放 guest socket。 |
| `asyncio` | 官方明确标注 **not WASI**；包入口导入 base_events、subprocess、threads 等，非 Windows 分支还导入 unix_events。 | Python `async`／`await` 和自定义 poll future 可单线程实现；不能因此声称完整 stdlib asyncio event loop、selector、网络／子进程 transport 已适配。 |

对应主源：[configure stub 选择](https://github.com/python/cpython/blob/v3.14.8/configure.ac#L4500)、[stub 宏](https://github.com/python/cpython/blob/v3.14.8/configure.ac#L4564)、[thread.c](https://github.com/python/cpython/blob/v3.14.8/Python/thread.c#L49)、[pthread stub](https://github.com/python/cpython/blob/v3.14.8/Python/thread_pthread_stubs.h#L6)、[线程创建失败](https://github.com/python/cpython/blob/v3.14.8/Python/thread_pthread_stubs.h#L82)；官方 [threading](https://docs.python.org/3.14/library/threading.html)、[multiprocessing](https://docs.python.org/3.14/library/multiprocessing.html)、[subprocess](https://docs.python.org/3.14/library/subprocess.html)、[asyncio](https://docs.python.org/3.14/library/asyncio.html)。

`--enable-wasm-pthreads` 是独立的、默认关闭的生产者配置；它改变 WASI target／链接与内存合同，不能无声加入本轮普通 `wasm32-wasip1` 产物，也不能从此选项的存在推断当前三个 runtime 已支持该路线。[配置开关](https://github.com/python/cpython/blob/v3.14.8/configure.ac#L1252)、[WASI 线程 target 与链接设置](https://github.com/python/cpython/blob/v3.14.8/configure.ac#L2256)

**候选标准库与 bundle／冻结依赖**

下表是固定版本的代表性直接依赖与延迟加载点，不是完整传递闭包，也没有实测 PASS。除已标注限制的模块外，可从纯 Python／builtin 组合评估具体操作；仍须证明所选构建确实包含相应 C builtin。仅冻结 encodings 和入口脚本，不能保证这些模块可用。

| 候选库 | 固定源码的依赖重点 | 必须额外核实的操作 |
| --- | --- | --- |
| `dataclasses` | 直接依赖 `re/copy/types/inspect/keyword/itertools/annotationlib/abc/reprlib`；3.14 的 `annotationlib` 是实际依赖，不能沿用旧版精简列表。 | 装饰器生成方法、默认工厂、`fields/asdict/replace`、字符串／延迟注解解析；不是只测试 import。 |
| `typing` | 直接依赖 `collections.abc/copyreg/functools/operator/types` 和 C builtin `_typing`；`annotationlib` 有延迟导入。 | `TypeVar/Generic`、ForwardRef、`get_type_hints`；注解求值引入的新模块与执行行为要单列。 |
| `weakref` | `_weakref`、`_weakrefset`、`_collections_abc`、`sys/itertools`；首次注册 `finalize` 延迟导入 `atexit`，退出清理还会导入 `gc`。 | 弱引用 callback、循环 GC、显式 close 与退出清理；不可把 finalizer 当成保证及时释放 ROS handle 的协议。 |
| `collections` | `_collections_abc/itertools/keyword/operator/reprlib/_weakref`；`deque/defaultdict` 来自 `_collections`，`Counter.most_common` 延迟导入 `heapq`。 | 精简构建不能假设所有导出类型都有纯 Python fallback；验证 pending deque 的 FIFO 和清理。 |
| `queue` | 直接依赖 `threading/types/collections.deque/heapq/time.monotonic`；可选 `_queue`。`Queue` 初始化使用 Lock 和三个 Condition。 | 即使不含 `_queue`，Python fallback 也没有消除 threading 等待依赖；非阻塞单线程操作与 blocking／join 语义分开资格测试。 |
| `asyncio` | eager 子模块闭包较大；`base_events` 依赖 `concurrent.futures/socket/subprocess/threading/traceback/weakref`，`unix_events` 还依赖 selectors、signal。 | 官方 not WASI；不要只冻结包入口，也不要把自定义 ROS polling loop 标为完整 asyncio 兼容。 |
| `traceback` | `collections.abc/linecache/textwrap/warnings/codeop/keyword/tokenize/io/_colorize/contextlib`。 | 异常类型／消息、真实 Python 文件行号和 locals 是不同能力；源码行文本还依赖 source loader 或源文件。 |
| `json` | 包入口依赖 decoder、encoder、codecs；decoder/scanner/encoder 依赖 `re`。`_json` 是可选加速，存在 Python fallback。 | UTF-8、空值、嵌套、非法输入、整数边界与精确输出；`re` 及 codec 的闭包仍须完整。JSON 不能自动替代已有 ROS bytes wire schema。 |

逐项源码：[dataclasses](https://github.com/python/cpython/blob/v3.14.8/Lib/dataclasses.py#L1)、[typing](https://github.com/python/cpython/blob/v3.14.8/Lib/typing.py#L19)、[typing 延迟 annotationlib](https://github.com/python/cpython/blob/v3.14.8/Lib/typing.py#L156)、[weakref](https://github.com/python/cpython/blob/v3.14.8/Lib/weakref.py#L11)、[weakref.finalize](https://github.com/python/cpython/blob/v3.14.8/Lib/weakref.py#L434)、[collections](https://github.com/python/cpython/blob/v3.14.8/Lib/collections/__init__.py#L27)、[queue](https://github.com/python/cpython/blob/v3.14.8/Lib/queue.py#L3)、[asyncio 包入口](https://github.com/python/cpython/blob/v3.14.8/Lib/asyncio/__init__.py#L5)、[base_events](https://github.com/python/cpython/blob/v3.14.8/Lib/asyncio/base_events.py#L14)、[unix_events](https://github.com/python/cpython/blob/v3.14.8/Lib/asyncio/unix_events.py#L3)、[traceback](https://github.com/python/cpython/blob/v3.14.8/Lib/traceback.py#L3)、[json](https://github.com/python/cpython/blob/v3.14.8/Lib/json/__init__.py#L99)、[JSON decoder](https://github.com/python/cpython/blob/v3.14.8/Lib/json/decoder.py#L3)、[encoder](https://github.com/python/cpython/blob/v3.14.8/Lib/json/encoder.py#L3)、[scanner](https://github.com/python/cpython/blob/v3.14.8/Lib/json/scanner.py#L3)。

几个容易漏掉的传递依赖：`inspect` 导入 `annotationlib/ast/dis/importlib.machinery/linecache/tokenize/weakref` 等；`annotationlib` 自身导入 `ast/builtins/enum/keyword/sys/types`；`reprlib` 使用 `_thread.get_ident`，`functools` 使用 `_thread.RLock`。因此“移除所有 `_thread` 依赖”会破坏本来可能正常的单线程标准库；应限制真实并发操作，并保留官方单线程 stub 的实际行为。[inspect](https://github.com/python/cpython/blob/v3.14.8/Lib/inspect.py#L139)、[annotationlib](https://github.com/python/cpython/blob/v3.14.8/Lib/annotationlib.py#L3)、[reprlib](https://github.com/python/cpython/blob/v3.14.8/Lib/reprlib.py#L5)、[functools](https://github.com/python/cpython/blob/v3.14.8/Lib/functools.py#L15)

冻结 bytecode 中的文件名不自动提供 traceback 的源码行文本。`linecache` 会尝试文件系统及 loader 的 `get_source`；frozen 名称还会查询模块 `__file__`。如承诺源码行展示，应显式提供原始源码／loader，或由桥提供真实 source cache，保留原文件名和行号。仅显示 C 解释器的 Wasm DWARF frame 不等于 Python frame；`sys.settrace` 的 Python 级桥与 WasmDBG 能力须分别记录。[linecache 源码来源](https://github.com/python/cpython/blob/v3.14.8/Lib/linecache.py#L98)、[loader.get_source](https://github.com/python/cpython/blob/v3.14.8/Lib/linecache.py#L186)、[Python 调试边界](research.md)

标准 CLI 加独立 stdlib bundle 仍可作为基准路线；精简 frozen launcher 应以真正使用的 API 构建闭包，并记录 Python 文件／hash、C builtin 清单、包 `__path__`、延迟导入和实际 `sys.modules`。没有验证的可选库不能通过改写成空操作而宣布支持；无 preopen 与显式只读 stdlib preopen 是不同部署合同。

**给 ROS SDK／rclpy 源迁移作者的约束**

1. 可评估单 guest 线程的 `poll → dispatch callback → poll` 执行器；列明 callback 顺序、公平性、timeout 的时钟／单位、取消、异常传播、重入、关闭和销毁后的行为。阻塞 callback 会阻塞该 guest 执行器，不能宣传 `MultiThreadedExecutor` 的并行保证。宿主 rmw／DDS 内部的真实线程不等于 Python guest 原生线程。
2. `async` callback／Future 需要明确由谁推进，以及等待期间是否继续处理事件。若不使用完整 asyncio，应明确支持的调度接口及不支持的 loop／transport 方法，避免仅依赖同名类带来错误兼容印象。
3. 上游 rclpy 的 native 扩展不能只靠复制 Python 源码得到。固定 WASI 配置不实现动态链接；ROS numeric imports 的静态 C／Java 桥属于单独的 SDK 适配，不能据此声称 native rclpy wheel、libffi／ctypes 或 DDS 可直接加载。[WASI 动态链接配置限制](https://github.com/python/cpython/blob/v3.14.8/configure.ac#L1237)
4. ABI 以实际 `sdk/abi/abi-v1.json`／C header 为准：memory index 为 i32，offset／length／handle 为 i64，输出 slot 为 8 字节 little-endian。Wasm32 C 指针使用 `(uint64_t)(uintptr_t)` 零扩展；P1 标准 I/O 自身仍使用其标准签名。UTF-8、bytes 所有权、长度、嵌入 NUL 与 borrowed buffer 生命周期需逐项验证；Python GC 对象地址不能直接作为 ROS wire buffer。
5. `weakref.finalize` 仅可作辅助：transport／capability／subscription 应有显式 close 和失败路径释放。所有回调与 future 注册需保持足够的强引用；关闭后不能再提交无效 handle。默认拒绝权限不因 stdlib 或 API 兼容层而扩大，`capability_open` 默认 deny errno 13 的合同保持原样。

**产物完成后的有限资格测试计划（本次未执行）**

- 分别记录 import、对象构造、实际操作；覆盖 dataclass 方法／注解、typing ForwardRef、weakref callback、deque、JSON UTF-8／非法输入、真实异常 traceback。记录每个操作触发的延迟模块，不能只依据一次 import 推导闭包。
- 如保留线程／进程／网络相关 API，使用有上限的负向测试保存实际异常或缺失方法；断言 callback 未执行、没有遗留线程／进程。线程数应区分 host runtime／DDS 线程与 guest 发起的线程，不能将 VM 自身 JIT worker 算成 Python 并发。
- 对迁移后的单线程 ROS executor 验证正常 polling、无事件 timeout、连续 callback 顺序、取消、callback 抛异常、销毁／重复关闭、default-deny 与空／UTF-8／嵌入 NUL bytes。只把实际成功的子集写入兼容表。
- Python 源级调试单列真实文件／行、函数名、初始化局部变量值及步进后更新；WasmDBG 单列 native Wasm frame／指令能力。rclpy 源迁移的完整性仍需对应固定上游版本、受支持 API 清单和真实集成测试，不由本文静态审阅推定。

# ROS 最近 LLVM 改动：原因、约束与复查入口

本文解释实现旁的维护注释，不把一个测试快照的成功扩大为全平台保证。
正式源码来源、逐文件补丁见 [vendor README](../../third-parties/llvm/README.md)
和 [UPSTREAM_CHANGES](../../third-parties/llvm/UPSTREAM_CHANGES.md)；实际执行结果及
未覆盖项目见 [稳定版验证记录](ros-bundled-llvm23.md)。

后续 cache v5、全 LLVM 注册目标、NaN 与 unwind 复核见 [本轮报告](ros-cache-v5-audit.md)。
运行时缓存的两版命名／格式隔离见 [隔离契约](llvm-jit-cache-product-isolation.md)；不要恢复旧跨产品布局。

## 为什么不能重新引入系统 LLVM 或 llvm-config

ROS 固定的是 **LLVM 23.1.1 正式发布源码 + 明确记录的下游后端／加载器修复**，不是
“任意版本号大于 23 的 LLVM”。相同版本号不能证明 MIPS 补丁存在；早期
23.0 开发版本还存在不同的 OptimizationLevel API 和 X86 有符号性组合问题。

`xmake/utility/bundled_llvm.lua` 校验源码清单，再用 CMake/Ninja 构建静态库。
缺少源码、版本不符、文件不符时必须报错，不能以系统安装作为回退，也不能
为通过检查而直接重生成清单。校验用于发现意外混用或陈旧文件，不是对拥有
构建工具、清单和工作区写权限的攻击者提供沙箱。

CMake 的 `uwvm_ros_llvm_contract` 只是元数据目标，**不编译、不链接、不执行**。
使用 executable 目标类型是为了让 CMake 计算完整的最终静态依赖顺序；静态库
目标本身只描述归档动作。这样交叉编译无需运行目标 ISA 的 llvm-config，
也无需在 Lua 中维护另一份可能漏项的 LLVM 依赖表。

必须保留 CMake 导出的库顺序、重复项、链接组及目标级链接选项。不要把它们
简单转成集合，也不要全部改成 `-lLLVM...`：前者可能破坏静态链接，后者会
重新允许搜索路径选中其他 LLVM。操作系统和 C++ 标准库仍可以动态链接；
“LLVM 静态链接”不等于“整个程序无动态依赖”。

`llvm/Config/llvm-config.h` 是版本和平台定义所需的公共头文件，不是已移除的
llvm-config 可执行程序。不能因为名称相似而删除它。

## 三类配置／缓存不要混淆

| 项目 | 原因和不可省略的约束 |
| --- | --- |
| 编译 LLVM 库的 Release 模式 | 优化宿主编译器实现；不是把所有 Wasm 强制设为 O3。CMake 工具链不能偷偷覆盖为 Debug，特别是 Windows CRT／迭代器 ABI。ROS 自身的 debug 模式与此独立。 |
| 磁盘中的 LLVM 构建缓存 | 身份包含源码清单、编译器字节、ABI 选项、后端和交叉配置；recipe 更新会重新配置，Ninja 判断哪些对象需要重编。stamp 不能替代源码检查和缺失产物检查。 |
| 运行时 Wasm 本机对象缓存 | 与 LLVM 构建缓存不是一回事。LLVM 的 `-uwvm-ros.6` 后缀参与代码生成身份；后端或加载器补丁更新不能悄悄复用旧补丁版本身份。这个下游修订号与运行时缓存格式 v5 是独立维度。 |
| xmake 进程内 memo | 只避免同一进程中每个测试目标重复哈希和构建。不能改成跨进程持久化的探测缓存，否则下次构建可能跳过变化检测。 |
| 构建目录锁 | 串行化独立 xmake 进程的配置、构建、元数据发布，不能只靠进程内 memo 防止竞争。 |

`--build-source-id=sha256:...` 是构建者对已核实源码清单的显式声明。xmake
仅检查格式，不会自动计算或验证外部清单；源码改变后必须更新该身份。为定位
模块缺失依赖而暂时冻结身份的增量诊断产物，只能用于隔离的测试与私有新缓存，
不能作为发布二进制或可分发缓存。签名验证也不能补救错误的源码身份。

缓存目录还必须在加锁／配置前解析成真实路径。传统头文件与模块构建可能通过
符号链接共用 LLVM；只按字面路径传递 `cmake -B` 会改变绝对 include 路径和
Ninja 命令摘要，导致同一套库重复全量编译。使用 CMake 的 `file(REAL_PATH)`，
不要依赖宿主系统的 Unix realpath 命令，也不要临时改变 xmake 进程的工作目录。
这只规范化同一缓存的路径，不放宽源码、ABI 或外来 CMake 缓存检查。

LLVM 使用 Release 优化并不允许 `-ffast-math`、`-Ofast` 或 `/fp:fast`。
LLVM 23 的 APFloat 用到共享 libc 数学辅助代码，宿主编译器若先丢掉 NaN、
infinity 或 signed-zero 语义，运行时的 Wasm 浮点环境保护无法补救错误常量折叠。
既检查 xmake 传入参数，也检查 CMake 工具链最终解析出的参数。
不能只读 `CMAKE_CXX_FLAGS`：工具链／项目钩子的 `add_compile_options()`
可通过目录属性加入 `-ffast-math`，而不改变该缓存变量。配置完成后，还检查
file-API 中实际 consumer 编译命令的 flags；检查在 `llvm-libraries` 构建前
执行。真实 CMake 负对照和生产 Lua reader 一起验证这条路径。该检查不把任意
可执行工具链脚本变成沙箱；逐文件任意改写规则的工具链仍属于受信构建输入。
还要先刷新 Ninja 的 `build.ninja` 目标：被工具链包含的文件变化时，Ninja
可能重新运行 CMake，旧 file-API reply 不能代表即将编译的参数。这个刷新
只生成构建规则，不编译 LLVM 库；库构建后再读取一次 reply，避免发布旧元数据。
Clang 的 `/clang:`／`-clang:` 和 `-Xclang=` 转发形式也要展开后检查：例如
`/clang:-ffast-math` 确实会定义 `__FAST_MATH__`，不能只拒绝直接拼写。

Windows 的 CRT 选择不能继续使用 `LLVM_USE_CRT_RELEASE`：LLVM 23 已不读取
这个旧选项，CMake 可能只给出 unused-variable 警告并继续构建，造成 ROS 的
MDd／MT／MTd 与 LLVM 默认 MD 混用。现在显式映射到
`CMAKE_MSVC_RUNTIME_LIBRARY`，并由 deferred hook 检查实际库目标的
`MSVC_RUNTIME_LIBRARY` 属性；不要只检查请求变量。Release 优化与 Debug CRT
是两个维度，保留 Release 不等于允许跨 CRT／标准库调试 ABI 混用。
GNU 风格 `clang++ --target=...-windows-msvc` 同样使用此 ABI，但 CMake 的
`MSVC` frontend 标志可能为假；必须同时识别 `CMAKE_CXX_SIMULATE_ID=MSVC`。
不能因此拒绝合法 Clang，也不能把 Windows GNU/MinGW 误认为同一个 CRT 模型。
`check_bundled_llvm_crt.py` 使用真实 CMake、Clang 和 COFF 对象检查四种选择的
`_DEBUG`／`_DLL` 宏及 defaultlib，并保留旧选项失效、目标属性覆盖的负对照。
它替换了 LLVM 库依赖图为一个无 SDK 的小型 fixture，没有链接 CRT 或运行 Windows。

安装的 Clang/GCC 及其 SDK 是 **bootstrap 编译器**，不是 ROS 链接的 LLVM。
尤其在原生 MIPS R6 上，vendor 内的补丁不会自动修好用于编译 ROS/uwvm-int
的系统 Clang；bootstrap 编译器仍须单独验证。QEMU 执行跨生成对象也不能
证明该目标完整 CLI、操作系统集成和 bootstrap 构建已经通过。

交叉工具链必须显式设置目标机器的 `LLVM_HOST_TRIPLE`，不能只设置 CMake
的 `CMAKE_SYSTEM_PROCESSOR` 或交叉编译器。实际 AArch64 GCC 配置曾在遗漏
该项时成功，但 LLVM 的 config.guess 仍观察构建机，导致 Native 选择 X86。
检查放在 LLVM 生成默认值之前；重新配置前也清除旧缓存中的推断 triple，
避免仅检查 DEFINED 就接受上次留下的错误值。后端列表必须包含 ROS 运行
机器的 native 后端；LLVM 的“仅警告、不生成 native JIT”行为不适合这里。

## 最近的 API／性能修改为什么保留这些边界

- `OptimizationLevel` 在正式版改成 enum，代码直接比较 O2/O3，保持之前的
  speed > 1 策略。不要猜 enum 序号，也不要按 LLVM major version 判断开发版
  与正式版的 API。no-inline／native-unwind 测试仍保留五种 speed/size 策略；
  没有 Os/Oz 的 API 用 O2 加 optsize/minsize，而不是删掉 size 测试。
- ROS 恢复直接 `SIToFP`／`UIToFP`，因为本次固定的正式 X86 后端已正确区分
  前后两次转换的有符号性；并非所有 LLVM 23 都没问题。此修改只影响整数到
  浮点的 IR 发出方式，不允许删除 NaN 位搬运、trapping／saturating 转换和
  FP 环境保护。普通 uwvm2 仍支持其他 LLVM，不能机械移除其兼容性修复。
- 保留 Wasm 函数的 noinline／nomerge／nooutline 和普通调用边界，是为了
  在各优化策略下保持运行时地址映射、递归回溯和 instruction 回退的一致性。
  它不等于给整个函数加 optnone，函数内部的优化、原生 SIMD 和合法融合仍要保留。
- 浮点“运算正确”与“原始位搬运正确”是不同义务。GCC/i386 `-O0` 的 x87
  浮点返回可能 quiet sNaN；`-msse2 -mfpmath=sse` 不会一并改掉普通 C++ 返回 ABI。
  不能以优化后内联隐藏了问题为理由改回按值 Float 返回。具体代码链和回归
  见 [浮点维护说明](../runtime/floating-point-change-rationale.md)。

栈入口缓存也不是“把保护关掉”。复核本地 WAVM 的
`6f871e61c6e8fc54fa5317b5d61d128d681846f3` 源码可见：
`POSIXPrivate.h`／`ThreadPOSIX.cpp` 用线程级初始化和备用信号栈，
`SignalPOSIX.cpp` 注册 `SA_ONSTACK`，`DiagnosticsPOSIX.cpp` 在需要时用
libunwind 遍历物理帧。libunwind 不替代内存／栈故障检测；WAVM 的 probe
属性也有目标条件，不能从这几处代码推导出所有 ISA 都有相同保护。
ROS 的嵌入契约还要求恢复宿主备用栈、检查会变化的主线程栈限制并处理线程
析构期间重入，因此仍保留这些检查。缓存节省分配与重复查询，不承诺和另一
引擎的一次初始化具有相同开销。具体代码、测量和覆盖边界见
[栈入口说明](../../test/0017.runtime/NATIVE_STACK_GUARD.md)。

性能归因必须固定源码、bootstrap 编译器和参数。此次改用通过初始化检查的
Clang 22 后，模块版相对旧 Clang 23 头文件产物的解释器微测慢约 5.8%；但
用同一源码、同一个 Clang 22 和同一套 LLVM 库重建头文件对照后，模块版在
相同微测中反而快约 4.7%，JIT 机器码和计时基本一致。前一种比较不能用来
断言模块修复导致退化，后一种比较也不能证明所有负载都加速。旧产物还来自
较早源码快照，不能把全部差异无证据地归因于编译器。保留原始数据与对照，
不要通过关闭初始化检查、NaN 保护或内存检查来追赶未经隔离的性能数字。

本地现成 WAVM 二进制来自 `build-ofast-lto-lld`，其 CMake Release 标志包含
`-Ofast`，且源码有本地修改。它不能未经单独正确性验证就充当“相同安全前提”
的正式性能基线；这里的源码比较不是本轮新的 WAVM 吞吐对比结果。

## 冷路径诊断也需要控制模板规模

完整模块构建还暴露了一个冷路径的模板膨胀：非法 full-policy 参数的单次
打印包含 17 个条件颜色操作，产生约 3.5 GiB 的 BMI。不能因为颜色共用一个
运行时开关，就假定编译器只实例化两种组合。现在拆成四个有界打印包；
**整条诊断仍持有同一把外层流锁**，内部通过未加锁的 `.handle` 打印，避免
对非递归互斥锁再次加锁。颜色操作仍保留为 manipulator，不能一律替换成
字符串：旧版 Windows 控制台可能需要修改控制台属性。

局部 RAII 锁也是有意的：fast_io 内部的 `io_lock_guard` 没有从其 named module
导出，头文件构建能看见它不代表模块构建也可以。此修改已同步普通 uwvm2；
没有修改策略选择、Wasm 热路径或 JIT 指令。回归入口为
`test/0018.build/check_callback_full_policy.py`，覆盖两个产品的不同诊断文案、
头文件／模块声明形式、GCC/Clang O0/O3、有／无颜色、六种合法策略及错误参数，
并检查多线程输出不会在四个打印包之间交错。这个隔离 fixture 替代了参数注册
和 usage-printer 外围依赖；不能把它当作完整 CLI 或 Windows 控制台测试。

## 模块导入不能代替功能宏准备

`loader` 的 WASI 初始化与分组校验还需要两种不同的依赖：`u8string_view`
必须直接导入 `uwvm2.utils.container`；`UWVM_IMPORT_WASI_WASIP1` 则必须在
global module fragment 中通过功能头准备，`import` 不传播宏。前者遗漏会使
完整模块构建失败，后者遗漏可能使构建成功却静默删掉初始化／校验。两版均
保留与头文件模式一致的依赖；不能以“多导入一个 WASI 模块”替代功能宏准备。
`test/0018.build/check_loader_module_features.py` 实际预处理 `loader.cppm`，
检查默认开启、显式关闭以及只移除功能头的负对照；它不等于完整 CLI 执行。

完整解释器的 SIMD 翻译也必须直接导入 `shared.wasm1p1_simd`。`optable`
导出的命名空间别名不等于重新导出该模块拥有的 `v128_unop` 等声明；头文件
构建的传递 include 会掩盖遗漏。ROS 曾在完整模块构建中因此失败，现已同步
普通版的直接导入并补齐对应头文件依赖。实际失败命令的隔离重放通过，单独
移除这条 import 的负对照仍复现原诊断；不要以增加一个别名来替代直接依赖。

SIMD 舍入汇编测试的标准库头也放在 `import` 之前，与生产模块的 global
fragment 顺序一致。当前 Clang 23／libc++ 组合在 import 后再包含 `<array>`
会报内部 `__promote_t` 重定义；这发生在代码生成之前，不是 SIMD 算法错误。
修正的是测试 fixture 的头文件顺序，两种模式仍调用同一套生产实现。

运行时 API 还有两层独立的模块约束，不能用增加 textual include 来互相替代：

- `run.cppm` 必须导入 `uwvm2.runtime`，与实现文件使用同一份 API／配置类型。
  在 global fragment 中包含 `uwvm_runtime.h` 时，模块拥有的 `u8string_view`
  尚不可见；再 textually 包含容器头则会混用类型归属。
- preload descriptor 的实际定义属于 `uwvm2.uwvm.wasm.type`。API 模块导入它，
  不在另一个模块中重复导出前置声明。传统头文件模式仍保留轻量前置声明，
  两种包含顺序都有效；结构布局、参数类型和宿主 ABI 没有变化。

运行时实现也必须直接导入内存诊断打印、global 引用类型和缓存哈希的声明。
协程函数定义需要在本翻译单元包含 `<coroutine>`；仅导入 `scheduled_task`
类型不能让标准库的 `coroutine_traits` 自动可见。上述修复同时用于普通版。
`check_existing_runtime_api_modules.py` 重放实际 API／host API 编译命令，验证
正确归属以及恢复旧前置声明后的失败；它不是链接／运行测试。轻量检查器现在
覆盖 runtime／SIMD 独立头文件及 implementation TU 的标准头，仍不替代编译器。

CRT 入口不再依赖 fast_io 未导出的 `net_service` 别名。原生 Windows 使用模块
已导出的同一具体 RAII 类型 `win32_wsa_service`，保留进入 Wasm 前的 WSAStartup
和返回后的 WSACleanup（含 Win9x）。POSIX／Cygwin／Wine 的原别名是空类型，
不需要构造空对象；不能把 Windows 的真实初始化也删掉。
`check_crt_network_service.py` 检查六种平台宏选择和实际导出表；非宿主平台行
只是预处理检查，不是 Windows SDK 构建或操作系统执行证据。

模块还具有真实的初始化对象，不只是提供声明的 BMI。完整链接暴露了 CLI
与 `uwvm_runtime` 各自登记同一批 `.cppm` 的问题：对象库依赖会把两份强符号
一起送入链接器，不能指望 inline／COMDAT 自动去重，更不能添加允许重复符号
的链接选项。现在生产模块对象由 `uwvm_runtime` 唯一拥有，CLI 及依赖该运行时
的测试目标消费它的公开接口。没有运行时依赖的独立测试仍自行登记接口。

xmake 的 `public = true` 在 Release 中也必须设置；它使依赖目标能找到模块，
不等于把 C++ 模块内部的私有声明 export 出去。运行时与消费者的 FP 参数不同
时，xmake 可以生成不同的兼容 BMI，但初始化对象仍只能链接一份。这次修复
没有把运行时的 FP 约束改成 CLI 的参数，也没有关闭模块初始化。
`check_module_object_ownership.py` 检查真实登记方式，并以小型 C++／xmake
工程验证单次初始化，以及恢复重复对象、隐藏接口后的两种失败。它不代替
整个 ROS 的最终链接和运行回放。

随后对未被引用的内部静态初始化进一步复核，确认测试机的 Clang 23 snapshot
存在独立的 bootstrap 缺陷：[LLVM #212170](https://github.com/llvm/llvm-project/issues/212170)，
上游修复为 [#218304](https://github.com/llvm/llvm-project/pull/218304)。不能把
“导出的变量初始化正常”当成这个问题已解决，也不能以切换单阶段编译或添加
`[[gnu::used]]` 作为未经验证的修复。本次单阶段候选仍复现，已撤回。

`module_initializer_check` 在模块配置时检查真实的 BMI → LLVM IR 路径，要求
保留内部非 inline 变量的初始化调用；只有声明存在不算通过。它不执行目标
程序、不依赖目标 libc，且仅在当前 xmake 进程缓存结果，避免编译器原路径升级
后仍信任旧探测。失败时保留小型复现并拒绝模块构建；传统头文件模式不受影响。
内置 LLVM 23.1.1 是被链接的库，不能修复用于编译 VM 的 bootstrap Clang。
当前 Linux Clang 22.1.8、macOS Clang 20.1.8 通过该探针；已知坏的 Clang 23
snapshot 被正确拒绝。版本号相同的其他编译器仍必须实际检查。

对象归属测试的编译器选择也必须与测试目标一致：xmake 内建 `clang` toolchain
会另行探测 PATH 上无版本号的 `clang`，即使传入了可用的 `--cc=/usr/bin/clang-23`
也可能在开始编译前失败。该小型夹具现在直接用指定路径定义自己的 toolchain，
不要求修改用户的全局符号链接；Linux 的“只有带版本号程序可见”与 macOS
路径选择均已验证。它仍只检查对象归属，不能替代独立的内部初始化探针。

自定义 libc++ SDK 的命令行也不能省略验证：在 xmake 的原始 `--cxxflags` 中
重复写独立的 `-isystem /路径`，参数去重可能移除前一个 `-isystem`，把目录
误当成另一个源文件。用 `-isystem/路径` 连写形式保留每一组对应关系。实际
失败的 dependency-scan 命令已复现，并验证此改法；这只是启动参数修正，
不代表改变了标准库 ABI，也不能用它绕过 LLVM 归档配置身份检查。

严格测试镜像不能继续保留过时的“LLVM 不支持”排除名单。ROS 原先跳过的七个
套件覆盖完整 Wasm 1.1、bulk memory、externref/table、table/ref bulk、基础
SIMD、无 else 的多值 if、验证器对齐；本轮七个均实际通过 LLVM runner 的
编译和执行，因此恢复为完整的 `strict/**.cc` 清单，与普通版本一致。
这些套件仍检查解释器翻译产物，只把执行改为 LLVM，所以仍要求同时启用两个
后端；移除排除名单不等于恢复任何 ROS 已删除的 lazy/tiered 模式。注册检查
增加了恢复过滤清单的负向控制；具体执行记录和快照范围见审计文档。

## 升级／回退时的最小复查清单

1. 只选官方非 prerelease 标签，记录准确提交、源码归档摘要和签名来源。
   比较 MIPS 补丁是否已经合入；不要无条件重复应用，也不能仅因版本更新就删掉。
2. 同步更新 Lua 版本、CMake 合同、`pinned_version.h`、后端测试的版本检查、
   README、UPSTREAM_CHANGES、源码清单及补丁修订后缀。用官方归档比较器核对
   全部保留文件，区分裁剪／符号链接物化与实际源码修改。
3. 运行 `test/0018.build/check_bundled_llvm_manifest.lua` 和
   `check_bundled_llvm_contract.lua`、`check_bundled_llvm_paths.lua`，再用 `check_bundled_llvm_cmake.py`
   检查实际 CMake Release 正控和 Debug 拒绝；该脚本只配置、不重复编译 LLVM。
   另查源码缺失时不回退系统、关闭 JIT 时不要求 LLVM、移走 llvm-config 后仍可构建。
4. 重跑 MIPS 的三个后端测试和 BE/LE O32 精确位执行；N32/N64 单独记录运行
   与仅生成对象的覆盖。重跑 X86 两种转换符号方向、32/64 位宽、O0/O3、SSE2、
   AVX2、AVX512（有／无 VL）、i686 x87/SSE2 和 x86_64 no-SSE 生成代码。
5. ROS 的验证器／翻译器一致性、Core 2 执行、SIMD 内存跨页与 grow、traps、
   五种优化策略回溯、签名对象缓存回放、栈耗尽以及 GCC/Clang O0/O3 位搬运
   都要检查；传统 include 和完整 named-modules 构建分别记录。
6. 用实际解释器汇编及已验证缓存对象中的 JIT 汇编检查高频路径。微测要区分
   编译／启动与稳定执行；QEMU 耗时不作为目标硬件性能证据。

参考检查入口：`check_validation_control_parity.py`、`check_wasm2_execution.py`
在 `test/0012.validator/`；内存、混合转换和回溯／缓存检查在
`test/0014.llvm_jit/`；`check_native_stack_exhaustion.py` 在 `test/0017.runtime/`；
解释器跨编译浮点矩阵为 `test/0013.uwvm_int/run_fp_cross_matrix.py`。
`test/0013.uwvm_int/check_existing_int_add_codegen.py` 可检查已有 x86-64 ELF
二进制中的选定标量／SIMD 加法实例，而不是重新编译 helper。它要求四组实例
都实际找到，检查原生 add、间接跳转及调用／栈寄存器使用；空选择不能算通过。
还拒绝不显式出现 RSP 的 push/pop/ret/enter/leave，普通直接跳转也不能冒充尾分派。
RBP 也可能是普通分配寄存器，因此报告中的栈寄存器命中需要进一步看汇编，
不能单凭失败就断定发生了 spill。该检查不是 no-SSE、全 ISA 或全部 opfunc 的判据。
Linux 栈缓存／信号转发／状态重置辅助回归可使用
`test/0017.runtime/check_runtime_guard_matrix.py`，覆盖 GCC/Clang O0/O3 和 UBSan。
它会执行故意耗尽栈的隔离子进程，应在统一资源限额内运行；这不替代完整 VM 测试。
这些驱动需要明确的 CLI、语料和目标 SDK／runner，跳过或构建失败不能计为通过。

`test/0018.build/check_int_compile_profiles.py` 可从一次成功的传统头文件语义
编译记录中取得目标参数，检查 combine 4 档 × delay-local 3 档 × reorder 2 档
× loop-unwind 2 档，共 48 个构建选项组合。它实例化现有 f64 严格测试的 tail
和 non-tail 翻译器，沿用 xmake **仅用于测试目标**的 `undefined-inline` 例外，
不能把该例外加到生产目标。这个检查不链接、不执行，不代表所有模板参数、
所有 ISA 或完整模块 CLI 的全部组合；运行测试和实际汇编检查仍须单独完成。

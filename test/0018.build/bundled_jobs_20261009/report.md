# ROS Xmake → CMake 并发传递（2026-10-09）

`llvm-build-jobs` 默认 `inherit`。外层 `xmake -jN` / `--jobs=N`
传入 LLVM 和配套 libc++ / libc++abi / libunwind 的 `cmake --build --parallel N`。
显式 `--llvm-build-jobs=N` 优先；独立 `xmake f` 的继承值为 1。
已有固定数值配置使用 `xmake f --llvm-build-jobs=inherit` 恢复继承。

Xmake 的 build 会进入 config 子任务，但不继承 jobs。生产 resolver
先读取当前 jobs；缺失时使用公开的 xmake.argv / option.taskmenu /
option.raw_parse 重新解析原始 build 参数，不访问内部 option context。
`inherit` 避开 Xmake 的特殊 auto 值处理。正整数上限为 CMake 接受的
2147483647；0、负数、false、分数、无穷大及溢出请求被拒绝。

LLVM 的固定编译池被清空；实际并发由每次 --parallel 决定。
LLVM 链接池仍为 1。并发数不加入 LLVM/runtimes 构建目录身份；源码中的
jobs 继续传给 bundled_runtimes.ensure，并由其两个 --parallel 调用使用。
改变 -j 不会仅因 jobs 不同创建新 LLVM 目录。

生产 Lua resolver / LLVM build 命令和实际产品选项默认值在真实
Xmake + CMake + Ninja 小型项目中验证。本机 17/17、Linux 17/17：
首次自动配置 -j1（子任务的当前 jobs 为空）、同目录改为 -j4、
长选项 3、显式目标的默认并发、b 别名 2、继承 32、独立配置 1、
固定覆盖 2、六种无效请求、恢复继承和长选项 1。
每个有效请求运行两组各 8 个自定义任务，记录开始/完成数及实际并发峰值；
LLVM 和配套 runtime 目标的峰值均为 min(jobs, 8)。所有请求使用同一个
CMake 构建目录。嵌套配置检查直接使用真实 Xmake 选项上下文。

macOS 自有进程 RSS 观测峰值 228,032,512 字节，低于 2 GiB；
Linux 使用现有 64 GiB / swap-zero cgroup，自有 RSS 峰值
173,506,560 字节。Linux 自有 PIDFD 全部 retired，OOM 计数未变化。
测试通过 XMAKE_STATS=false 禁用额外统计任务；此前被监督器拦下的
统计 git 子进程记录保留在 Linux control/ 中，未计入通过结果。

复现入口为 `check_bundled_llvm_jobs.py --out <新目录> --xmake <路径>`，
应在资源监督器内运行。生产脚本哈希和完整结果见 results.json。
此检查使用小型自定义任务代替 LLVM 编译，没有重新执行完整 LLVM/ROS
编译或最终链接，也没有运行真实 libc++/libunwind 编译。

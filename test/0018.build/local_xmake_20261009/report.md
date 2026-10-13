# 本机 xmake -j1 测试记录（2026-10-09）

两个主程序均未完成编译/最终链接：保持之前的本机 2GiB 限制后，实际编译触发了提前停止。不能把资源中止写成源码编译错误，也不能据此宣称放宽内存后一定能成功。

| 仓库 / 配置 | 结果 | 最后编译单元 | 本测试进程树观测峰值 MiB |
|---|---|---|---:|
| uwvm2 / Release O3 / uwvm-int + LLVM | 内存提前停止 | `src/uwvm2/uwvm/main.default.cpp` | 1801.22 |
| uwvm2 / split O0 / uwvm-int + LLVM | 内存提前停止 | `src/uwvm2/uwvm/host_api.default.cpp` | 1871.05 |
| uwvm2 / split O0 / LLVM-only | 内存提前停止 | `src/uwvm2/uwvm/host_api.default.cpp` | 1865.83 |
| ROS / Release O3 / uwvm-int + LLVM | 内存提前停止 | `src/uwvm2/uwvm/main.default.cpp` | 1797.31 |
| ROS / split O0 / LLVM-only | 内存提前停止 | `src/uwvm2/uwvm/host_api.default.cpp` | 1879.69 |
| ROS / pinned runtime + LLVM source build | 通过 | `—` | 1267.16 |
| ROS / native runtime smoke | 通过 | `—` | 176.47 |

ROS 的 `xmake f` 实际使用 CMake + Ninja，从校验过的固定源码构建 libc++、libc++abi、libunwind，再构建 LLVM 库（2261 个步骤，内层 jobs=1）。这不是重建一个 clang 编译器；已有 clang 仅作为 bootstrap 编译器。源码依赖这一阶段通过，用时约 43.3 分钟。随后的 `xmake -j1` 主程序编译实际执行过。

uwvm2 使用已有 LLVM 的 llvm-config 和外部 runtime 配置；没有改成 ROS 的源码依赖构建流程。未进入最终链接，不能把外部 unwind 的最终绑定算作已验证。

默认 Release O3 双后端，以及现有 split O0 低内存路径、LLVM-only 路径都实际测试了。后两种路径同样在 `host_api.default.cpp` 触发提前停止。仓库原来的 .xmake 配置没有被本测试写入；测试使用 XMAKE_CONFIGDIR 和独立输出目录。

本轮在 ROS 构建脚本修复了：

- Python 头文件生成器写入 pinned source 的 __pycache__，导致下一次严格源码校验失败；保留严格校验，为子进程设置 PYTHONDONTWRITEBYTECODE。
- Darwin CMake 只收到 CMAKE_SYSROOT 后仍选择 CLT SDK；同步指定 CMAKE_OSX_SYSROOT。
- Apple typed allocation compatibility：编入未修改的 pinned upstream shims，并保留必要符号。
- CMake configure probe 把 -Xlinker 操作数当库名/去重；分开真实库列表和完整 SHELL 链接参数组。
- bootstrap Clang 的 PCH 名称查找失败；禁用 LLVM PCH，未修改 LLVM 源码。
- 实验中的 force_load 全 archive 在 Darwin lld 产生重复零长度 compact-unwind 条目，导致最简单 throw/catch 失败；最终使用必要符号的 -u 保留，移除了 force_load。

新增 native regression 实际通过：静态初始化、整数 throw/catch、嵌套帧析构、OS 线程异常捕获、exception_ptr 跨线程重抛。二进制不导入系统 libc++，Unwind_RaiseException、Unwind_Resume、gxx_personality_v0、cxa_throw 均由自编译库解析。该测试验证这些实际路径，不代表所有 VM/Wasm 行为已测试。

可重跑该回归（应在本机资源监督下运行）：

```sh
python3 test/0018.build/check_macos_paired_runtime.py build/bundled-runtimes/c35e40c15c70e09862cc048ffa017c0e93551ec4a1d61fcf381bf2a14fe6499b /tmp/uwvm-ros-runtime-probe
```

ROS 本轮明确设置 `--llvm-runtimes-provider=bundled --llvm-build-jobs=1 --use-llvm-compiler=y`。当前 auto + use-llvm-compiler=false 不会启动 bundled runtimes；外层 -j1 不会自动覆盖内层默认 jobs=2。因此不能把任意已有配置的裸 xmake -j1 都说成必然自编译 unwind。

内存监督使用进程出生标识和实际后代查询，阈值 1792MiB，为采样间隔及排队监督器预留空间；这是 RSS 采样/提前停止，不是 macOS 内核硬限制。config8 曾因历史 PID 重用使监督器失败，该尝试完全排除在通过结果外；修正监督器后 config9 从新的库缓存完成了完整源码构建。所有最终 receipt 的残留子进程列表为空。

仅清理本任务创建并已退役的六个缓存，约 663MiB；保留当前通过的源码库缓存，不删除其他任务的 build 数据。

result.json 包含实际命令、资源回执、源码 manifest 哈希和 runtime 链接检查。owned-fixes.patch 以本轮开始时的文件内容为基线，保留既有其他修改；不是相对 Git HEAD 的完整仓库 diff。

# WASIp1 checkpoint：线程登记槽复用（R41）

已同步修复 uwvm2 和 uwvm2-ros。旧代码把活动线程槽和旧句柄识别记录放在同一组 256 个槽中：即使线程已经完成 OS/TLS join，只要调用方保存旧句柄，新线程仍可能得到 quota。这会阻碍长期运行和后续恢复线程创建。

现在，256 个槽只保存尚未实际 join 的原生线程；独立的弱引用记录负责识别旧句柄，并复用失效记录。弱记录不额外保留原生线程、启动状态或 source。识别先比较 control block，再核对真实对象地址；伪造地址和另一个 control block 均被拒绝。两个实际工厂共用登记及创建失败清理方法；ROS 模式条件保留。

查找和创建失败清理避免临时取得无关旧线程的强引用，防止调用方并发释放最后一个句柄时，在登记锁中触发无关旧世界的析构。新增可分配操作位于 OS create 前；实际 join 路径未增加分配。生产 OS 线程仍使用 FastIO，新增测试输出及源文件加载仍使用 FastIO。

## 实际验证

- 修复前真实运行复现 1 次：保留 256 个旧句柄、实际 join 一个线程后，下一次启动仍得到 quota=4。这个预期失败不计入通过用例。
- 修复后 24/24 Linux 原生用例：登记压力 4、Core 退休 8、WASIp1 退休 8、活动 generation 2 嵌套帧 4。
- 压力用例实际创建 2340 个原生线程，执行 1316 次最小 Wasm 调用并返回 42；原 Core/WASIp1 退休和 generation 2 用例保留完整 fixture。覆盖 256 活动线程限制、线程体结束与 TLS 尚未完成时拒绝复用、OS/TLS join 后复用槽位、保存旧句柄后继续创建、旧句柄重试、失效弱记录复用及伪造句柄拒绝。
- generation 2 用例仍验证两层活动帧、私有代码所有权、再次 checkpoint 及原世界嵌套继续执行结果 1190。
- 16 项前端检查：两仓库各在 Linux、Windows、FreeBSD、macOS target 编译 runtime 和新测试。后三个 OS 本轮未原生执行。
- 14 个 WAT 输入分别 assemble 和 validator 校验，共 28 项 Wasm 数据检查。这不表示已经覆盖全部 Wasm 3.0 功能。

## 未完成项和资源约束

仍需联合 world 发布，实际联合采用 source/initializer/GC/WASIp1/code，发布前预创建恢复线程，关闭入口下完成所有根及 ledger 安装，打开入口并在新世界回放。私有 restored 工厂登记辅助逻辑仅得到编译验证，实际 restored-world 启动尚未接通。

1190 属于同一个保留世界的继续执行，不证明内存、文件或 GC 状态回滚。Wasm checkpoint 必须同时 checkpoint WASIp1；跨 OS 恢复需要重新配置 mount，状态包不包含文件内容。

所有编译、validator 和原生测试在原 64 GiB Linux cgroup 中完成。本轮产物使用 768 MiB 有界 RAM 目录，aggregate RSS 上限 6 GiB，持久输出上限 16 MiB。原持久四 OS 套件磁盘策略未放宽。最终合格测试进程均已物理退出，RAM 产物已清理，父 cgroup memory.events 未改变。

v1/v2 harness 准备失败、v3 新增 fixture 编译失败、v4 为避免锁中无关强引用而主动中断，均保留记录并排除。v5 为替换无关的长循环压力 fixture 而主动中断，亦排除。结论来自 v6 冻结源码及最终 guard。详见同名 JSON 和归档中的原始命令、日志、源码散列及资源证据。

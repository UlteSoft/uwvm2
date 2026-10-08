# Boolean 显式转换修复：阶段记录（2026-10-06）

uwvm2 与 uwvm2-ros 同步修复了共享数值表达式中已分类 Boolean 的显式 cast：
非零复制位先规范化为 1，再转到目标整数/f32/f64。
例如复制 bool 位为 2 时，(int)flag 得到 1，
static_cast<double>(flag) 得到 1.0；普通整数值 2 仍转换为 2/2.0。
改动使用已有 promote 纯数值路径，没有增加 guest/host 访问能力或改写原始 DWARF 数据。

已观察到两个仓库的最终原生回归通过，各 24 阶段：
11 次编译、10 个当前组件运行、3 个旧 header 预期失败。
新组件各 1,295,317 条检查，category 247,159 条；
原有 scalar/conditional/character/Zig 等回归和 DAP 202 个正例、60 个负例通过。
用有效原生 bool 值作转换对照；非规范位只作为复制的 DWARF DATA，
没有向原生 bool 对象写入非法表示。

跨架构已观察 x86_64、aarch64、i686、riscv64 的两个版本通过，共 8/32 组合。
ppc64 普通版随后失败（runner returncode=1），实际编译或运行错误尚未取得；
此后 SSH Linux 连接和 Tailscale ping 超时，阻断失败日志读取和剩余测试。
不能把 ppc64 或剩余组合标作通过，完整跨架构资格尚未完成。

本记录依据工具已返回的 guard stdout 和原生摘要；
SSH 原始完整日志/ELF/回执位于 /run/user/1000/uwvm2-bool-cast-20261006-a2，
当前无法重新读取，也尚未建立这些远端原始证据的完整持久化副本。
本机阶段证据只保存本轮 12 个配对 scoped 文件、before/差异/阶段记录，
不能替代完整远端证据归档。

实现和上述测试的范围为有限复制标量/声明类型与 DAP 语法、console bridge。
完整当前 VM/JIT、真实 stopped-frame、语言 producer、modules build
和完整各语言原生调试体验本轮未重新验证。
同窄条件表达式、Zig bool @as 拒绝、原有权限和资源保护保持原有行为。

转换规则参考 [C11 N1570，6.3.1.2–6.3.1.4](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)。

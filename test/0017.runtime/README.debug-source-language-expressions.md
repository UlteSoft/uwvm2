# Read-only language expressions in the full debugger

`-Rdbg` uses the LLVM full backend. A source query requires embedded DWARF,
a genuine cooperative Wasm stop, and its current thread and stop identifiers.
Use `bt THREAD` to obtain the current stop ID. The same interfaces are present
in uwvm2 and uwvm2-ros; ROS exposes only the uwvm interpreter full and LLVM full
execution backends.

The console retains whitespace in the complete expression:

```text
print THREAD STOP shadow + 2
print THREAD STOP (packet.grid[1][2] + shadow) * 2
print THREAD STOP sizeof(packet)
print THREAD STOP shadow == 7 && 0xff == 255
print THREAD STOP p->next->value
print THREAD STOP p.next.value
print THREAD STOP slice[1]
print THREAD STOP pair.0 + pair.1
print THREAD STOP p.* + 1
print THREAD STOP text
ptype THREAD STOP packet
print-frame THREAD STOP FRAME shadow + 2
print-frame THREAD STOP FRAME -shadow
print-frame THREAD STOP FRAME p.* + 1
ptype-frame THREAD STOP FRAME packet
```

`p.next.value` implicitly dereferences conventional Rust reference types whose
producer type name begins with `&` only in an original Rust compilation unit.
Go pointer dot selectors use an original Go compilation unit; TinyGo uses its
C99 compilation unit with the exact TinyGo producer adapter, including unnamed
pointer types. C/C++ pointers use `->` or unary `*`; genuine C++ DWARF reference
types also support dot selectors. Rust `&[T]` indexing recognizes its exact `data_ptr`/`length` layout and
checks the signed index, length, pointer carrier availability, address width,
and complete guest range before reading an element. Qualified Rust `&str`, Go
and TinyGo string layouts display at most 4096 copied bytes in one bounded
read. A longer valid string displays its prefix with an explicit truncation
marker. The complete declared range must fit the Wasm address width even when
only a prefix is displayed. Empty strings need no memory read; failed, short or
oversized copies leave text unavailable. C/C++ character pointers retain their
separate bounded string path. Terminal control bytes are escaped through the
existing metadata formatter. The [2026-10-06 regression record](README.debug-string-preview-20261006.md) distinguishes owned DATA checks from actual
producer and VM qualification. The [sequence provenance/range record](README.debug-sequence-provenance-20261006.md) adds language-CU gates and whole-slice address-width checks.

Rust numeric tuple/tuple-struct selectors map `.0`, `.1`, etc. to the actual
rustc `__0`, `__1` DWARF fields only in an original Rust compilation unit.
Exact/alias collisions remain ambiguous, offsets come from producer metadata,
and unavailable bits remain unavailable. Zig postfix `p.*` uses the same
bounded conventional guest-pointer plan as unary `*`, including member chains
and scalar expressions. Optional `.?` is still unavailable. DAP watch/hover
accepts the postfix selector under its existing syntax and stop checks.

Integer expressions support decimal, hexadecimal, binary (`0b`) and octal
(`0o` or leading zero) literals, `true`, `false`,
parentheses, unary `+ - ~ ! *`, `+ - * / %`, shifts, comparisons, equality,
bitwise operators, Go `&^` at multiplicative precedence, short-circuit `&& ||`,
and `sizeof(source-object-expression)`. Integer digit separators (`'` or `_`)
must lie between digits; `_` is also allowed immediately after an explicit
radix prefix. Repeated and trailing separators are refused.
Explicit radix prefixes and C `U/L/LL` suffixes retain the existing guest-width
candidate rules. Invalid radix digits and values exceeding 64 bits fail during
FastIO scanning; arithmetic never interprets an invalid literal as another name.
Integer promotions and signed/unsigned widths are derived from copied producer
scalars; `sizeof` uses the guest address width. Invalid shifts, divide by zero and signed overflow fail without C++
undefined behavior. The numeric subset also supports f32/f64 arithmetic and
bounded builtin numeric conversions with C/C++/Rust/Go syntax, plus the existing
finite Zig `@as` coercion. Calls, assignments, address-of, pointer casts and
arbitrary host addresses remain unsupported. This is a finite
read-only expression grammar, not a complete evaluator for every source language.

Finite Zig `@as` checks use the same source-shape, declared-width and literal-range
rules for value evaluation and for type inference in either arm of the shared
C-style conditional. A dead arm such as `@as(i16, value)` with a declared i32
source, or `@as(u8, 256)`, cannot silently become a supported result type.
Only owned direct literals, or one negation of an unsuffixed numeric literal or
an existing finite ASCII character literal, supply constant bounds; type callbacks
never supply value bits. Unsupported compound operands and C casts are refused before
a value resolver is called. Declared numeric categories are preserved separately from storage width.
DWARF signed/unsigned eight-bit integers now support safe same-type/widening
coercions, including u8 to i16 and nested @as results. Boolean sources of every
supported storage width remain unsupported for numeric @as. Unclassified legacy
eight-bit carriers remain conservative. A declaration/value category mismatch,
even with identical width/sign/float fields, publishes no value;
Unicode, compound CTFE and complete Zig language semantics remain unavailable.
The [Zig reference](https://ziglang.org/documentation/master/#Type-Coercion-Integer-and-Float-Widening)
explains widening and representability. These checks preserve the finite
subset's existing limits; the debugger's `?:` grammar is not Zig `if` syntax.

When a stopped-frame caller supplies a declaration/type resolver, root and nested
Zig `@as` conversions validate the declared source before invoking a value resolver.
Unsafe declared conversions are rejected without value access; unknown metadata
returns unavailable without borrowing a copied value as its declaration. A read
value must then agree with that declaration in width, signedness and integer/float
category, even when both types could fit the destination. Direct owned literals
need no symbol metadata, and logical short circuit performs no new metadata query.
The explicit value-only DATA API (including a const `no_type_resolver`) retains
its earlier behavior; it provides no actual guest frame or stop authority.

The finite Zig constant subset now accepts one negation of an unsuffixed numeric
literal or an existing finite ASCII character literal, including `@as(i8, -128)`, `@as(i64, -9223372036854775808)` and
`@as(f32, -0.0)`. Integer bounds use the owned literal magnitude before the shared
C-style literal promotion rules; signed minima do not overflow a host signed
integer, negative nonzero values cannot become unsigned, and floating negative
zero retains its sign. The same bounds apply to type inference in an unselected
conditional arm. This adds neither symbol-based CTFE nor compound constant
evaluation. Unary `+`, nested signs, typed/suffixed literals and negated runtime
variables remain outside this Zig coercion subset. The Zig reference documents
[negation](https://ziglang.org/documentation/0.15.2/#Table-of-Operators) and
[representable compile-time constants](https://ziglang.org/documentation/0.15.2/#Type-Coercion-Compile-Time-Known-Numbers).
Existing ASCII code point literals such as `@as(i8, -'A')` also supply owned integer constants; the
[Zig code point type](https://ziglang.org/documentation/0.15.2/#String-Literals-and-Unicode-Code-Point-Literals)
is `comptime_int`. This does not extend the shared parser to Unicode literals.
Ordinary C-style unary expressions outside `@as` retain their earlier behavior.

The finite character subset accepts one ASCII character or one of the existing
escapes `\n`, `\r`, `\t`, `\0`, `\\` and `\'`. For example, `' '` evaluates
to 32, `'\''` to 39, and `'A' + '\n'` to 75. The closing quote is checked at the
exact next byte; `'a '` and `'\n '` are refused rather than reduced to a single
character. Bare apostrophes and raw line breaks are also refused, including in
an unselected short-circuit or conditional operand. This shared numeric grammar
does not implement Unicode character literals or native language-specific
character/rune typing. C++ conditionally supports
[multicharacter literals](https://eel.is/c++draft/lex.ccon); this debugger subset
continues to refuse them. The character regression component, DAP corpus and
`run_debug_character_literals.py` check supported values, refused syntax and
10,000 independently computed arithmetic properties using actual target ELFs.

The finite C-style numeric conditional operator `condition ? yes : no` is
right associative and has lower precedence than `||`. Both arms must have
supported, declared result types; only the selected arm reads values or
performs arithmetic. For example:

```text
print THREAD STOP decimal32 > 0 ? decimal32 : decimal64
print THREAD STOP 1 ? 7 : 1 / 0
print THREAD STOP 1 ? 7 : sizeof(packet)
print THREAD STOP 0 ? 7 : sizeof(*null_packet)
```

`sizeof` uses authenticated type metadata for complete supported aggregates,
arrays and conventional guest pointers, including in an unselected arm. An
unselected `sizeof` does not invoke the value resolver or read guest bytes.
Missing declarations, unknown extents and bit-field operands are refused.
The result width follows the Wasm32/64 guest ABI. Conditional pointer/aggregate
results, overloaded conversions, C++ reference/glvalue results, and same
narrow or boolean arm types still require additional language semantics.
These limits apply to this numeric subset; Rust and Go conditional syntax has
its own language requirements. The C++ working draft specifies
[conditional evaluation and result types](https://eel.is/c++draft/expr.cond)
and [unevaluated sizeof operands and bit-field restrictions](https://eel.is/c++draft/expr.sizeof).

Pure scalar expressions and `sizeof` consume one authenticated stopped position and
owned copied numeric/type metadata. They do not require a successful checkpoint
recording or a linear memory. A reachable guest-memory operand restarts the complete
expression inside ONE runtime source-memory transaction; short-circuit operands
never trigger a read. Plain scalar roots retain the existing copied-local and
direct-constant provenance path. Every genuine activation, source binding, participant cohort,
function generation, frame selection and applicable guest-memory check remains mandatory.
Arithmetic never creates a pointer read plan. All query results own their bytes;
no VM pointer or borrowed runtime memory is retained. Limits include 4096 input
bytes, 128 syntax nodes, 32 nesting levels and 32 guest-copy operations per query.

DWARF paths are compared lexically after normalizing separators, `.` and `..`.
This does not open a source file or resolve a host symlink. Breakpoint selection
uses statement rows, prefers a matching `prologue_end` row and chooses the nearest following
statement when optimization erased the requested line. It excludes rows
marked `epilogue_begin`. DWARF transient row flags reset after each emitted row.
An optimizer can still attribute code to an earlier source line before a variable
has been initialized; the debugger does not invent a later value for that stop.

TinyGo metadata compatibility preserves the exact union of overlapping ranges
within one DIE. Conflicting location lists are quarantined as unavailable, so
unrelated functions can remain usable. A Wasm32 discarded range with an unrepresentable
end at or above `2^32` retires that DIE's entire coverage. Ordinary malformed and out-of-Code
ranges still fail. A reversed inline-only interval whose endpoints are both inside Code
retires that entire inline scope; its children cannot inherit the physical parent
range or publish storage. Other malformed ranges remain rejected. An empty own range never inherits a live ancestor range.

Terminal managed shutdown independently authenticates the actual private cancellation,
participant, source epoch, generated activation cleanup and precise GC-root cleanup.
A failed checkpoint observer may retain stale owned history; only after those
checks does shutdown discard the failed recording data. Its failure stays sticky,
and capture, GC census and restore remain unavailable. Healthy recording cannot
be discarded by this path. Execution drain and physical worker join still finish
separately before CLI shutdown completes.

Standard Go's Wasip1 compiler currently omits embedded DWARF. AssemblyScript's
external JavaScript source map carries line locations but does not supply the
DWARF type/local location metadata required by this interface. These producers
need a separate metadata adapter; this change does not claim full native
Go/Delve or AssemblyScript language-debug parity. Optimized-out objects,
unsupported layouts and ambiguous frame/location descriptions stay unavailable.

Regression components are `debug_source_scalar_expression`,
`debug_source_language_expression`, `debug_source_language_sequences`,
`debug_source_dwarf_index`, `debug_source_map` and `debug_source_step_policy`.
The new production text parsing, binary decoding, string construction and
formatted output use fast_io. Runtime qualification must use an actual fresh
product for each repository inside the designated SSH Linux cgroup.

DAP watch/hover now validates the same finite numeric syntax before sending one explicit `print-frame` query. Builtin conversions, sizeof and character-literal spaces are preserved. The complete actual stop/frame is checked before and after copying, and the real `source-stop` or `source-value stop=...` formatter envelope is checked before publication. Contiguous `++` and `--` are rejected by both the adapter and the C++ parser; spaced binary/unary signs remain ordinary read-only arithmetic. This covers syntax/copied display contracts and does not by itself qualify live runtime stops or native language parity.


The current frame controller packs both declared and read scalars through
`source_scalar_expression::from_dwarf_numeric`. It retains the exact DWARF
integer/boolean category on both copied-local and owned guest-object paths;
it does not grant new reads or runtime authority. Owned numeric casts and
`@as` results retain numeric categories so that `@as(i64, @as(u8, 255))`
and `@as(i16, @as(i8, -128))` are supported. Boolean literal syntax and
C-style boolean/native conditional language rules keep their existing limits.
[Zig widening](https://ziglang.org/documentation/0.15.2/#Type-Coercion-Integer-and-Float-Widening)
requires the destination to cover the whole declared source type; a small current
value cannot justify narrowing or changing signed integers to unsigned integers.

Classified booleans in the shared C-style arithmetic subset promote to signed
32-bit int and normalize nonzero copied bits to one, regardless of DWARF storage
width. This does not implement language-specific native boolean result types.
The DWARF packing helper rejects invalid storage widths and mismatched f32/f64
kind-width pairs before they can supply numeric type evidence.

Explicit shared numeric casts now normalize a classified DWARF Boolean before
integer or f32/f64 conversion, independently of its storage width. For example,
a copied nonzero Boolean representation converts through (int)flag,
static_cast<double>(flag), f32(flag) or flag as i64 to the numeric value one.
The original copied metadata/bits are not rewritten. Genuine integers and legacy
unclassified wider callbacks keep their original magnitude; @as from bool stays
unsupported. This is the shared finite C-style numeric cast layer, not complete
Go/Rust/Zig language-specific conversion semantics or a new live-VM qualification.

The Linux QEMU component recipe accepts independent --deps-root and --qemu-root
providers. Omitting --qemu-root uses --deps-root for QEMU as before. Private
GNU cross-linker native library directories are applied only to the compile/link
subprocess environment and pinned before/after; target guest library paths remain
explicit. [Cross-provider recovery and actual ppc64/x86_64 evidence](README.debug-cross-provider-recovery-20261006.md)
records the guarded component scope and resource limits.

Decimal floating literals now accept apostrophe/underscore separators only
between successive digits in the integer, fractional and exponent segments.
Examples are 1'234.5, 1_234.5, .1_25f and 0.15e+0_2. The controller normalizes
owned syntax into a bounded buffer before fast_io::parse_by_scan; DAP preserves
the original expression. Misplaced separators are rejected even in dead syntax.
[Float-separator implementation and actual native/QEMU evidence](README.debug-float-separators-20261006.md)
records compiler-oracle bit checks, existing numeric/DAP regressions and scope.

See [floating literal range consistency](README.debug-float-range-20261006.md) for DAP/C++ binary32 overflow and nonzero underflow boundaries, exact decimal checks, and the guarded native/ppc64/x86_64 qualification.

Grouped producer references now keep their selection path through parentheses inside scalar expressions. Examples include (*p).field + 1, ((pair)).0 + 1, ((p)).* + 1 and ((array)[1]).field + 1. The combined selection bound stays 32 steps; suffixes on arithmetic/cast/conditional numeric results are rejected. [Grouped postfix implementation and guarded paired native/QEMU evidence](README.debug-grouped-postfix-20261006.md) records the path-signature oracles, short-circuit behavior, existing regressions and scope.

Named numeric casts now accept token whitespace, including static_cast < int > (value), @as (i32, value), (unsigned   char)(value) and sizeof(unsigned   char). Builtin type words remain separate tokens; arbitrary calls/pointers and mutation remain refused. [Conversion spacing implementation and paired guarded native/QEMU evidence](README.debug-cast-spacing-20261006.md) records the old comparison-parse defect, independent numeric/ABI oracles and retained regressions.

Builtin integer casts and sizeof now accept standard type-specifier combinations in any order, including signed, unsigned short int, unsigned long long int and long unsigned int. Duplicate/conflicting words remain rejected; long uses the authenticated guest width, and Zig @as retains its separate builtin set. [Type-specifier implementation and paired guarded native/QEMU evidence](README.debug-builtin-type-specifiers-20261006.md) records compiler type-equivalence oracles, malformed inputs and retained regressions.


Distinct narrow conditional operands (2026-10-07): declared integer widths/signs
that differ, or a Boolean paired with a classified integer, now use common
arithmetic promotion. Examples: `1 ? (signed char)-1 : (short)2` and
`0 ? true : (unsigned char)255` return signed 32-bit results. Both arms require
type metadata, only the selected arm reads values, and copied Boolean storage
normalizes to truth. Equal narrow representation, two Boolean arms and
unclassified narrow carriers remain refused; C++ exact type/glvalue and C
language rules still need separate authority. See
[scoped compiler/QEMU regression evidence](README.debug-conditional-distinct-20261007.md).


Selected-frame language semantics (2026-10-07): actual C/Objective-C scope
metadata now selects integer promotion for same narrow/Boolean conditional
operands. Actual C++/Objective-C++ scope metadata selects Boolean results for
literals/comparisons/logical operations and two Boolean conditional branches.
TinyGo C99 + exact TinyGo producer is excluded. Unknown language and cross-CU
origin scopes keep the conservative shared dialect. C++ same narrow integer
identity/glvalue rules remain unavailable. These rules refine the earlier
same-representation refusal only for the specified language/profile cases.
See [compiler, actual LLVM and QEMU evidence](README.debug-language-semantics-20261007.md)
for examples, 40 fresh Wasm metadata producers and limitations; this cut does
not qualify a fresh full -Rdbg stopped guest session.


Exact narrow copied values and C23 constants (2026-10-07): C++/Objective-C++
conditional expressions now retain a narrow integer representation with exact
standard builtin identity or a matching canonical DWARF type DIE. Equal
width/sign alone is insufficient. Single ordinary ASCII character literals
have the finite default-Wasm `char` type in the C++ profile. Explicit
`DW_LANG_C23` selects Boolean predefined constants while comparison/logical/
conditional operations retain C integer rules; old C CU codes cannot select
that version automatically. This refines the preceding exact-narrow refusal
for proven copied scalar results; full lvalues/CV/reference/ptype and complete
language parsing remain unfinished.
See [native compiler, real LLVM metadata and QEMU evidence](README.debug-exact-narrow-20261007.md).
This cut does not qualify a fresh full -Rdbg stopped session or other SDK architectures.


C++ narrow variable/cast bridge and copied names (2026-10-07): actual bounded
same-CU base-type metadata now permits standard narrow variables to match
builtin casts and independently emitted validated standard base DIEs.
Computed copied scalar names retain `char`, `signed char`, `unsigned char`,
`short` and `unsigned short` when proven. Typedef display names do not provide
that proof; atomic wrappers hidden within aliases remain excluded. Selected
copies still match their inferred declaration key, and common primitive
results never invent a DIE key. This refines the preceding variable/cast and
independent-DIE refusal; full ptype/CV/reference/glvalue and complete native
language parsing remain unfinished. See
[paired metadata, native and QEMU evidence](README.debug-narrow-bridge-20261007.md).
No fresh linked full -Rdbg stop/session or additional SDK architecture is qualified here.


Standard integer rank and copied names (2026-10-07): finite selected C/C++
profiles now preserve int/long/long long and their unsigned counterparts from
actual bounded base metadata, standard casts and finite literal candidates.
Promotion/unary/shift/arithmetic/conditional result names retain their proven
standard rank, including equal-width int/long on Wasm32. Unknown aliases do
not acquire rank; atomic/cross-CU/non-C producers retain prior exclusions.
This adds canonical primitive copies, not full ptype/CV/reference/glvalue or
all native language grammar. See
[native types, real LLVM metadata and three QEMU architectures](README.debug-integer-rank-20261007.md).
No fresh linked full -Rdbg session or all-architecture product parity is qualified here.


Unevaluated compound sizeof (2026-10-07): existing fixed-size scalar expression
syntax now supports sizeof(arithmetic/cast/conditional/nested-size operands).
Bounded selected-frame type queries infer extents without value/sizeof callbacks
or arithmetic execution, including unavailable values and division/shift errors.
Exact size_t identity, user-type/VLA/function sizeof, full native grammar and
live -Rdbg/all-architecture qualification remain unfinished. See
[sizeof implementation, real metadata and QEMU results](README.debug-sizeof-expression-20261007.md).


Unary sizeof followup (2026-10-07): sizeof now owns a single existing unary
operand without mandatory parentheses. Binary/conditional precedence, metadata-only
compound typing and direct declaration extent routes are preserved. DAP reserved
sizeof roots cannot bypass the scalar grammar; identifier prefixes remain valid.
Exact size_t/full native/live product/all-architecture parity remain unfinished.
See [unary sizeof implementation and qualification](README.debug-sizeof-unary-20261007.md).


ASCII character escapes followup (2026-10-07): ordinary ASCII numeric octal/hex
escapes and standard simple escapes now use one complete bounded character token
for scalar parsing and sizeof delimiters, mirrored by DAP before broker IO.
Independent native/Wasm compiler witnesses and real metadata/QEMU components
cover the finite C/C++ profile; Unicode/full native/live/all-architecture parity
remain partial. See [implementation and qualification](README.debug-character-escape-20261007.md).


Native C/C++/C23 logical operands now use bounded declaration preflight before
value evaluation, with selected copied-type consistency and no dead value/extent
reads. Invalid dead arithmetic types and missing declarations are refused;
well-typed dead division remains unevaluated. The shared numeric lazy contract
is preserved. Current compiler/real Wasm metadata/protocol/QEMU evidence and
remaining native/live limitations are recorded in
[logical preflight qualification](README.debug-logical-preflight-20261007.md).


Standard Wasm size_t rank followup (2026-10-07): finite native C/C23/C++ profiles now carry canonical unsigned-long sizeof and builtin size_t cast identities on standard Wasm32/64, with native usual/conditional conversions and no operand value reads or invented DIE. Shared/TinyGo/unknown ABI remains generic. Fresh compiler/embedded metadata/native/QEMU and DAP regressions are in [the size_type report](README.debug-size-type-20261007.md). WALI, complete scoped aliases/CV/reference/grammar, linked/live product and remaining architectures remain unfinished. Historical qualification fields retain their original source-cut scopes.

Extent contract followup (2026-10-07): native declaration-only and evaluated sizeof now share Boolean/category/guest-width/representability checks, while scalar operand-type callbacks and shared_numeric behavior retain their previous contracts. Native/real Wasm metadata/controller frontend and x86_64/PPC64/AArch64/RISC-V64 QEMU component results are in [the extent report](README.debug-size-extent-20261007.md). Complete native/live product and remaining architecture parity remain unfinished.

Fresh full-product regression (R32, 2026-10-07): paired real LLVM-full -Rdbg Wasm32/64 stops now qualify the stated C-family sizeof/type subset, including 4000 sustained sessions and actual replacement validation/stale metadata refusal. This is a finite frozen-source result; native language parity, other language/architecture products and merged DWARF5 verification remain unfinished. See [full product evidence](README.debug-language-product-r32-20261007.md).


Go/TinyGo string/slice len/cap followup (R33, 2026-10-07): paired finite builtins and DAP grammar now have original compiler-oracle CLI/soak qualification, plus native/QEMU component checks. Ordinary len/cap variable-name whitespace lookahead is now freshly linked into both final frozen-source full VM products and qualified by TinyGo replay, authentic C variable stops and the original C-family matrix (88 sessions / 1508 comparisons across the pair). The preceding 197-session soak retains its preceding source-cut scope. Array/map/channel/Rust .len and full native Go semantics are unfinished. See [the R33 report](README.debug-go-builtins-r33-20261007.md).

Go/TinyGo fixed-array and pointer-array len/cap followup (R34, 2026-10-07): both freshly linked frozen full LLVM products now qualify finite named/nested/zero-length/zero-size-element arrays and nil pointer-array constant queries with original TinyGo O0 compiler oracles. The same final products pass 92 short sessions / 1628 comparisons and a further 177 sustained sessions / 5310 comparisons (10 minutes per repository). Real QEMU DATA components pass 450 array + 145 descriptor assertions per profile across 16 Linux profiles in each repo, 64 target ELFs total. Array constant evaluator paths do not read operand values; controller stop/frame/root authentication remains required. Map/channel, shadowing, general native Go/Rust semantics, full Go DAP IDE sessions, current ROS provider parity and full architecture VM/JIT parity remain unfinished. R33 qualification remains historical to its source cut. See [the R34 report](README.debug-go-arrays-r34-20261007.md).


TinyGo runtime-backed map/channel followup (R35, 2026-10-07): finite map len and no-scheduler channel len/cap, including original named/nil/empty/closed/directional channel cases, now pass paired frozen full-product source stops and same-product 10-minute soaks (131 sessions / 3930 value comparisons), plus 96 short regression sessions / 1748 comparisons. Six original compiler arguments and fixture/Go-spec invariants are distinguished. Sixteen Linux QEMU profiles per repo execute 96 real target ELFs with 1402 collection + 450 array + 145 descriptor DATA assertions per profile. Nil and type component paths do not read headers; nonnil copies use the existing qualified guest-pointer transaction and never follow bucket/buffer/queue/function pointers. Other layouts/producers, map entries, shadowing, full native Go/Rust semantics, Go DAP IDE, current ROS provider and full architecture VM/JIT parity remain unfinished. Historical R33/R34 results retain their own source scopes. See [the R35 report](README.debug-go-collections-r35-20261007.md).


R36（2026-10-07）补齐并实际验证有限 TinyGo map/channel DAP prepared-attach：三种 evaluate context 返回有限整数显示，Hover/type 协商、首条 JIT 有界等待及失步 VM 通道退休；两仓库各 82 单元测试和超过 10 分钟实际 DAP 测试通过。Go aggregate/完整 IDE/native/cross-VM 仍未完成。详见 [README.debug-go-dap-r36-20261007.md](README.debug-go-dap-r36-20261007.md)。

R37（2026-10-07）补齐有限复制对象 DAP 展开：真实 TinyGo *box Watch/Hover/Variables 展开 11 字段、只读分页与停点退休；两仓库各 96 项回归及超过10分钟真实 DAP 测试通过。自动 aggregate Source variables/完整 Go/其他 producer 和 native/cross-VM 仍未完成。详见 [README.debug-source-objects-dap-r37-20261007.md](README.debug-source-objects-dap-r37-20261007.md)。

R38（2026-10-07）补齐实际 TinyGo 当前源码帧 box 的 Source variables 根指针显示与按需 11 字段只读展开，两仓库各 110 项回归及超过10分钟真实 DAP 测试通过；不从 guest bits 或复制字段名追踪对象。其他语言布局/map entries/完整 Go/native/cross-VM 仍未完成。详见 [README.debug-source-scope-dap-r38-20261007.md](README.debug-source-scope-dap-r38-20261007.md)。

R39（2026-10-07）补齐实际 TinyGo 固定/具名/二维/零尺寸数组 DAP 元素与分页资格，并修复完整复制字符串在 Watch/Hover/Variables evaluation 中显示协议文本的问题。两仓库各 118 项回归和超过10分钟真实 DAP 测试通过。其他语言/自动字段 payload/map entries/native/cross-VM 仍未完成。详见 [README.debug-array-text-dap-r39-20261007.md](README.debug-array-text-dap-r39-20261007.md)。

R40（2026-10-07）补齐有限整数枚举的 DAP 数值/名称叶子与含枚举对象展开，C/C++ 实际 Wasm32/64、DWARF4/5、双调用栈策略和超过10分钟持续测试通过。另修复 Objective-C/Objective-C++ 缺省数组下界，真实 LLVM metadata component 通过；新完整 VM/DAP 尚待验证。详见 [README.debug-enum-dap-r40-20261007.md](README.debug-enum-dap-r40-20261007.md)。


R41 后续：Objective-C / Objective-C++ 默认数组下界修复已在 R35 frozen full source + 修复的新完整 VM 上通过实际 Wasm32/64、DWARF4/5 DAP 和长测。当前全部工作区及最新 ROS provider 仍待资格化。参见 README.debug-objc-full-dap-r41-20261007.md 和 debug_objc_full_dap_qualification_20261007.json。


R42（2026-10-07）：官方 Rust Wasm32 O0 DWARF4/5 的负判别值载荷、空变体、Option<NonZeroU32> Some/None 已通过原 R41 两完整 VM 的实际 DAP 展开、分页、真单步退役与超过10分钟会话。132 unit tests/仓库及 C 系语言、TinyGo 回归通过。完整 Rust native、优化/Wasm64 和完整跨架构 VM/JIT/DAP 仍未资格化。见 [README.debug-rust-variants-dap-r42-20261007.md](README.debug-rust-variants-dap-r42-20261007.md)。


### R43 Rust 活动变体摘要

折叠变量和直接 object.FIELD 求值现在显示已复制活动 case/完整有限载荷，例如 Negative { signed: -3 }、Some { __0.__0.__0: 13 }、None；原类型、只读属性及全部子树保留。超过有限摘要预算时仅显示已证实的 case，不增加查询或标签派生 selector。真实 Rust Wasm32 O0 DWARF4/5 两栈策略、持续会话和最终整合回归见 [R43 报告](../../test/0017.runtime/README.debug-rust-variant-summaries-dap-r43-20261007.md)。完整 Rust pretty-printer/native 等价、优化布局、Wasm64 和全架构 VM 仍未资格化。


### R44 Rust 嵌套活动变体

有限嵌套enum/Option复制摘要已支持两层/三层实际活动case，例如 Carry { payload: Negative { signed: -3 } }，数组载荷显示items[0]。每层都验证实际复制selection，整个外层共享节点/深度/值/UTF-8预算；完整子树保留，不生成查询或指针权限。真实编译器样例、两栈策略、持续会话与回归见 [R44报告](../../test/0017.runtime/README.debug-rust-nested-variants-dap-r44-20261007.md)。完整Rust pretty-printer/native、其他layout、Wasm64和全架构VM仍未资格化。

R45实际Wasm32 O0 DWARF4/5 DAP验证支持 object.pair.0、object.nested.0.1、object.arrays[1].1、(*object.next).pair.0、Rust as i32及tuple子对象展开/分页；真实C/C++同名__0/__1字段不会跨语言启用数字别名。computed unsigned int现在在合法32位范围内呈现标量，其他C primitive/未知ABI不新增资格。参见[R45报告](README.debug-rust-tuples-unsigned-dap-r45-20261007.md)。

R46真实C系O0 DWARF4/5 Wasm32/64 DAP检查八种copied整数primitive及其有符号/无符号边界；原guest编译表达式自检对照，完整短测矩阵与600秒持续测试通过。保留明确guest位宽和只读属性，不推导未知ABI或增加写权限。参见[R46报告](README.debug-c-integer-dap-r46-20261007.md).

R47完成C系canonical copied bool/float/double的DAP显示。真实原Clang guest表达式自检对照包含次正规数、最大有限值、Inf/NaN、负零及C/C++比较结果类型；完整矩阵和至少600秒持续测试通过。不据此声称其他语言native布尔语义完成。参见[R47报告](README.debug-c-scalar-dap-r47-20261007.md).

R48 已补 Rust/Go/TinyGo/Zig 的有限 primitive bool 谓词、严格逻辑操作数、Rust整数 !、Go/Rust优先级，以及DAP的Zig and/or语法。两仓原始类型组件及genuine Rust/TinyGo会话已验证；完整native语言/类型推断、其他producer和full QEMU仍未完成。见 [README.debug-native-predicate-r48-20261008.md](README.debug-native-predicate-r48-20261008.md)。

R49 最新primitive谓词在16个Linux QEMU profile的组件覆盖及公共SDK/linker入口修复见[跨架构报告](README.debug-native-predicate-cross-r49-20261008.md)；完整VM/JIT/DAP与native体验仍未资格化。

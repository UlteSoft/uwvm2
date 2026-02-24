# Contributing to uwvm2

This document defines the engineering conventions for uwvm2. The priorities are portability (freestanding-first), performance, determinism, and maintainability. All contributions must follow these rules unless explicitly waived by maintainers.

### Language, toolchain, and targets
- Use modern C++ (C++26) with modules and carefully selected library components that are available in freestanding environments.
- The runtime layer (`src/uwvm2/runtime/`) relies on C++26 features such as pack indexing (`Type...[i]`); ensure the toolchain defines `__cpp_pack_indexing >= 202311L` and build with `-std=c++26`.
- Prefer freestanding builds where possible; guard hosted-only features with preprocessor macros.
- Compiler warnings should be clean with, at minimum: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wuninitialized -Wunused -Wmisleading-indentation` (or the closest equivalents across toolchains). Some content uwvm2 does not provide redundant warnings during construction, but it is best to enable these recommended warnings during development to ensure that errors are less likely to occur.

### File, directory, and naming conventions
- Headers use the `.h` extension. Do not use `.hpp`.
- In `src/`, implementation files use `.cpp`. In `test/`, test sources use `.cc`.
- For each module group that needs translation-unit code with preprocessing, split source files into two files per group:
  - `<name>.module.cpp` (or `module.cpp` in a leaf directory): imports-only + wiring, then `#include` the matching `<name>.default.cpp`.
  - `<name>.default.cpp` (or `default.cpp`): default implementations and preprocessor-dependent code.
  This split is required because module import processing precedes preprocessing (imports are processed before `#if`).
- Module interface units use `.cppm`.

### Module/header import rules
- Each `.cppm` must have a corresponding `.h` that mirrors its imports in the `#ifndef UWVM_MODULE` block. The content and order must match one-to-one.
- Category ordering for imports/includes (if present) must respect the policy in section “Import/include category ordering (expanded)”; the ordering must be identical between the `.cppm` and its header mirror.
  Notes:
  - `uwvm_predefine/*` is its own category and must not be mixed into `utils/*` even if the path contains `utils`.
  - Use fully-qualified dotted names for module imports (e.g., `uwvm2.utils.container`), and matching `<.../impl.h>` includes in headers.
  - Local partition headers such as `"def.h"` correspond to partition imports like `import :def;`.
- Aggregator pairs `impl.cppm`/`impl.h`:
  - In `impl.cppm`, expose submodules with `export import ...;`.
  - In `impl.h` (inside `#ifndef UWVM_MODULE`), include the matching `sub/impl.h` or local headers. Relative includes like `"sub/impl.h"` map to fully-qualified `base.module.sub`.

See Appendix A for minimal module/header templates.

To validate these rules run: `python3 test/0006.check_module/check_uwvm_module.py`

### Runtime layer conventions (`src/uwvm2/runtime/`)
- Follow the standardized file prologue used by neighboring runtime files (copyright/license block + `@author`/`@version` banner). Do not invent new prologue styles.
- Use the runtime include structure:
  - include groups with explicit headings such as `// std`, `// macro`, `// platform`, `// import`;
  - include `.../macro/push_macros.h` near the top (and feature push macros when needed);
  - include the corresponding `.../macro/pop_macros.h` at the end of the file.
- Prefer project portability/annotation macros in hot code: `UWVM_ALWAYS_INLINE`, `UWVM_NOINLINE`, `UWVM_MUSTTAIL`, `UWVM_THROWS`, and `UWVM_IF_NOT_CONSTEVAL` (see `src/uwvm2/utils/macro/*` and the `src/uwvm2/runtime/**/macro/*` layers).
- Module/export bridge pattern for headers:
  - headers default `UWVM_MODULE_EXPORT` to empty and may use it to prefix namespaces/declarations;
  - `.cppm` units define `UWVM_MODULE_EXPORT` as `export` before including the header mirror.
- Threaded-interpreter hot paths should remain leaf in tail-threaded mode. Avoid introducing cold `call` edges that force prologues on AArch64; use tail wrappers for traps where appropriate (e.g., `trap_integer_divide_by_zero_tail` in `src/uwvm2/runtime/compiler/uwvm_int/optable/numeric.h`).

### Namespaces and qualification
- Across namespace boundaries, use fully-qualified names:
```cpp
namespace A { void f(); }
namespace A { inline void ff() { return ::A::f(); } }
```
- Inside a single namespace scope, unqualified calls are allowed:
```cpp
namespace A { void f(); inline void ff() { return f(); } }
```

### Standard library and freestanding policy
- Default to freestanding-friendly facilities. Avoid non-freestanding headers prior to their freestanding adoption and toolchain maturity.
- Prefer `::uwvm2::utils::container::string` and other project-provided containers over `::std::string` and non-freestanding STL containers in core layers to keep behavior controlled and portable.
- Prefer `<cstdint>` integer types and explicit-width aliases. Default to `::std::size_t` for sizes and indices.
- Use `::std::addressof` for addresses of objects where operators may be overloaded.
- Exceptions are discouraged in low-level code and freestanding contexts. Prefer explicit error codes and RAII for lifetime management. If exceptions are used in hosted layers, document propagation and cost.
- IO policy is centralized; see section “IO and file handling (expanded)”.

### Formatting policy: avoid fmt-based formatting (fmtlib, ::std::format, ::std::print)

Avoid fmt-based formatting in uwvm2. Prefer `fast_io` for formatting and output:

- Control/data-flow separation enables better optimization in compilers.
- Benchmarks built against `fast_io` trunk/next using non-inlinable `ret` stubs to isolate userspace buffering overhead show `fast_io` outperforming fmt-style formatting under equivalent conditions.
- Mixed-type output benefits from inlining, generating smaller code.
- Automatic coalescing merges multiple outputs into a single scatter/gather operation, reducing context switches.
- Smaller footprint suitable for freestanding/constrained targets.

If a hosted facade is necessary, build it atop `fast_io`, keep the API narrow, and document locale/allocation semantics.

### Concept-Oriented Programming (COP) in uwvm2
- uwvm2 applies concept-oriented design (per Alexandr Savinov) using modern C++:
  - Separate concepts (interfaces/constraints) from implementations using C++ `concept` and module partitions.
  - Model "concept spaces" as module namespaces; implementations are imported via explicit module graphs rather than inheritance webs.
  - Favor value semantics and generic algorithms constrained by concepts; avoid deep inheritance and fragile virtual hierarchies.
  - Use type erasure or tagged unions only at well-defined boundaries where dynamic polymorphism is required.

### Coding style and formatting
- Follow the repository `clang-format` configuration. When formatting causes pathological output, temporarily disable formatting with the standard `clang-format off/on` comments around the smallest possible region and add a brief justification comment above it.
- Style rules:
  - Prefer explicit, descriptive names. Avoid 1–2 character identifiers outside tight local scopes.
  - Minimize macros; prefer `constexpr`, templates, and inline functions.
  - Keep functions short and cohesive; use early returns; handle errors and edge cases first.
  - Comments should explain "why", not restate code.

### Platform guidance
- Guard platform-specific code with robust feature tests/macros. For Windows use clear guards and avoid including `windows.h` in public headers. Import and wrap platform APIs explicitly; prefer RAII wrappers over raw handles.

### Testing and verification
- Unit and property tests live under `test/` and use `.cc` extensions.
- Prefer fuzzing for parsers and binary formats; integrate sanitizers in CI.
- Before submitting, ensure:
  - Module/import conformance check passes: `python3 test/0006.check_module/check_uwvm_module.py`.
  - Build is warning-clean.

### Commit discipline
- Keep edits cohesive and logically scoped. Write descriptive commit messages that state the intent and the user-visible impact.

### Rationale highlights (portability and performance)
- There are no truly zero-cost abstractions; design and measure deliberately.
- Freestanding-first constraints guide API choices (containers, IO, exceptions). Prefer libraries and patterns that work across toolchains and environments without hidden dependencies.

If in doubt, open an issue before investing in a large change.



---

## Deep modules conformance guide

This section specifies normative rules, examples, and anti-examples for C++26 modules in uwvm2.

### Normative rules
- Each interface unit `.cppm` must have a `.h` mirror with one-to-one, order-preserving imports-includes mapping inside `#ifndef UWVM_MODULE`.
- Interface unit and header mirror must preserve the category order defined earlier. Do not interleave categories.
- Interface units must not contain preprocessor-dependent behavior prior to the `#ifndef UWVM_MODULE` header-bridge block. Conditional logic belongs in `default.cpp` / `*.default.cpp`.
- `module.cpp` / `*.module.cpp` is restricted to wiring, `import` statements, and module-level scaffolding. No platform conditionals, no heavy logic.
- Partition imports (e.g., `import :detail;`) must correspond to local headers (e.g., `"detail.h"`) in the mirror block.
- Aggregator modules expose only stable submodule surfaces via `export import`; transient or experimental modules must not be exported from aggregators.

### Example: correct interface/mirror pairing
```cpp
// src/uwvm2/utils/foo/foo.cppm
export module uwvm2.utils.foo;
import fast_io;                       // utils/*
import uwvm2.utils.container;         // utils/*
import uwvm2.utils.hash;              // utils/*
import :impl;                         // local partition
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "foo.h"
```

```cpp
// src/uwvm2/utils/foo/foo.h
#pragma once
#ifndef UWVM_MODULE
// mirror the imports in the same order
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include "impl.h"
#endif
```

### Example: aggregator layout
```cpp
// src/uwvm2/utils/foo/impl.cppm
export module uwvm2.utils.foo:impl;
export import :algo;
export import :adapters;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#include "impl.h"

// src/uwvm2/utils/foo/impl.h
#pragma once
#ifndef UWVM_MODULE
# include "algo/impl.h"     // uwvm2.utils.foo.algo
# include "adapters/impl.h" // uwvm2.utils.foo.adapters
#endif
```

### Anti-examples (do not do this)
- Reordering mirrored includes in `.h` because of perceived include speedups.
- Putting `#ifdef _WIN32` branches in `.cppm` before the header bridge.
- Exporting unstable submodules from an aggregator.

### Rationale
Strict mirroring ensures that non-module consumers receive an equivalent dependency graph and that hybrid builds behave identically. The two-file split (`*.module.cpp` including `*.default.cpp`) eliminates subtle bugs introduced by the modules import-before-preprocessing rule.

---

## File layout examples

### Example: utilities subpackage
```
src/uwvm2/utils/string/
  impl.cppm
  impl.h
  string.cppm
  string.h
  module.cpp
  default.cpp
  algo/
    impl.h
    algo.cppm
  adapters/
    impl.h
    utf8.cppm
```

### Example: parser wasm base
```
src/uwvm2/parser/wasm/base/
  impl.cppm
  impl.h
  types.cppm
  types.h
  reader.cppm
  reader.h
  module.cpp
  default.cpp
```

The directory hierarchy mirrors module names (`uwvm2.parser.wasm.base` → `src/uwvm2/parser/wasm/base/`). Aggregators (`impl.*`) live at the directory root and expose cohesive partitions.
In some directories (notably `src/uwvm2/runtime/lib/`), the same split is written as `<name>.module.cpp` and `<name>.default.cpp` to avoid ambiguous filenames and to make cross-directory grepping easier.

---

## Import/include category ordering (expanded)

The category order applies globally and locally in every unit:
1) third-party libraries
2) `uwvm_predefine/*` (standalone category)
3) `utils/*`
4) `parser/*`
5) `validation/*`
6) `object/*`
7) `imported/*`
8) `uwvm/*`
9) `runtime/*`
10) local headers (same directory) and remaining items

Notes:
- Do not mix `uwvm_predefine/*` into `utils/*` ordering.
- Always use dotted module names in `import` and path-based includes in headers.
- Local partition headers must match partition imports exactly.

---

## Qualification policy (expanded)

### Fully qualify across namespaces
```cpp
namespace A { void f(); }
namespace A { inline void ff() { return ::A::f(); } }
```

### Unqualified within the same namespace
```cpp
namespace A {
  void f();
  inline void ff() { return f(); }
}
```

### Additional guidance
- Avoid relying on ADL in cross-namespace calls; it obscures ownership and harms readability.
- Qualify names when calling into third-party namespaces to aid grepping and refactoring.
- Prefer explicit `::std::` qualification for standard facilities in headers to avoid surprises from ADL and using-directives.

---

## Freestanding adoption policy (expanded)

### What “freestanding-first” means
- Design APIs that compile and run without hosted-only facilities.
- When hosted facilities are necessary, isolate them behind minimal adapters with clear contracts.
- Track standard and vendor support before relying on newly freestanding-marked headers.

### Containers and strings
- Prefer `::uwvm2::utils::container::string` instead of `::std::string` in core layers for deterministic behavior and portability.

### Integers and character types
- Default to `::std::size_t` for sizes and indices.
- Prefer `<cstdint>` fixed-width or least-width types where widths are not guaranteed.
- Treat character types (`char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`) as integers; avoid implicit locale-transforming operations in core code.

---

## Memory and allocation (expanded)

### Heaps are not universal
- Do not assume a default heap exists or behaves uniformly. Some targets expose multiple heaps; some expose none.
- Avoid `::std::nothrow`/`::std::nothrow_t`; library implementations typically route through throwing paths internally.

### Allocation failure
- Prefer fail-fast at appropriate boundaries (e.g., `::std::abort`) rather than attempting universal recovery from OOM.
- Recognize that many dependencies and OS subsystems will terminate on OOM regardless of caller policy.

### 128-bit arithmetic
- Prefer two-limb arithmetic using `::std::uint_least64_t` pairs rather than toolchain-specific 128-bit integers when portability is required.

---

## IO and file handling (expanded)

### Prefer `fast_io`
- Use `fast_io` for deterministic, high-performance IO.
- Avoid iostream/stdio in core code paths due to locale coupling, buffering surprises, and binary size costs.

### Memory mapping for large files
- Prefer memory mapping to `seek+read` for large inputs. Mapping reduces copies and exposes contiguous ranges.
- Provide explicit fallbacks for platforms lacking mapping (e.g., WASI) behind uwvm2 IO abstractions.

### Format-string safety
- Never pass untrusted strings as format strings. Always separate format and data. See “Formatting policy” for the preferred `fast_io` approach.

---

## Exceptions and error handling (expanded)

- Avoid exceptions in low-level and freestanding layers due to portability, binary size, and performance concerns. Implementations vary (SJLJ vs. table-based EH), and both can degrade optimizations and locality.
- Prefer explicit error codes and RAII for resource management. Consider result types that carry status and value explicitly.
- Keep error domains well-defined and avoid mixing transport (e.g., OS error codes) with semantic errors.
- In hosted boundary layers where exceptions are used:
  - Document which exception types may escape.
  - Define the mapping between exceptions and error codes at boundaries.
  - Preserve strong or basic exception-safety guarantees explicitly.
  - Avoid throwing across module boundaries that are consumed by freestanding code.

---

## Windows guidance (expanded)

### Guards and headers
```cpp
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
// windows-specific code
#endif
```
Do not include `windows.h` in public headers. Import only the required APIs with the correct calling conventions and attributes, and wrap resources in RAII types.

### Unicode and resources
Prefer W (Unicode) APIs on NT-family systems. CRT file descriptors wrap HANDLEs but are not interchangeable; provide clear RAII wrappers and avoid `::std::unique_ptr` for HANDLEs.

### Threading ABIs
Do not assume `<pthread.h>` availability on Windows (ABI variants: win32, posix, mcf). Use portability layers or platform primitives via abstractions.

---

## Style, formatting, and documentation (expanded)

### clang-format
- Follow repository configuration strictly. If formatting becomes pathological, disable it around the smallest possible region with standard on/off comments and add a brief justification comment immediately above. Re-enable right after.

### Naming and APIs
- Functions: verbs/verb phrases.
- Variables: nouns/noun phrases.
- Avoid cryptic abbreviations; prefer clarity and consistency.

### Control flow and structure
- Keep functions short and cohesive; use guard clauses and early returns.
- Handle error and edge cases first.
- Minimize macros; prefer `constexpr`, templates, and inline functions.

### Comments
- Explain intent and invariants. Avoid restating obvious code.

---

## Validation and CI expectations (expanded)

- Run `python3 test/0006.check_module/check_uwvm_module.py` to validate module/header conformance.
- Build with warnings enabled and fix all diagnostics or justify narrowly scoped suppressions.
- Add/extend unit tests in `test/` (`.cc`) and fuzzers for parsers and binary formats.
- Use sanitizers (ASan/UBSan/MSan where applicable) during development and in CI.

---

## Frequently used checklists

### New module
- [ ] `.cppm` with correct module name and imports
- [ ] `.h` mirror with exact, ordered includes inside `#ifndef UWVM_MODULE`
- [ ] `impl.cppm`/`impl.h` if aggregating partitions
- [ ] `module.cpp` / `*.module.cpp` (imports-only) and `default.cpp` / `*.default.cpp` (preprocessor-dependent code)
- [ ] tests in `test/` (`.cc`)
- [ ] module conformance checker passing

### Platform-specific change
- [ ] robust preprocessor guards
- [ ] no public `windows.h` includes
- [ ] RAII wrappers for OS resources
- [ ] platform-gated tests

---

## COP in practice (expanded)

- Concepts define contracts; implementations reside in partitions imported explicitly.
- Module graphs express composition via `export import` instead of inheritance hierarchies.
- Use dynamic polymorphism (type erasure/tagged unions) only at narrow boundaries.

Example sketch:
```cpp
export module uwvm2.io.reader:concepts;
export template <class R>
concept Reader = requires(R r) {
  { r.read_some() } -> ::std::same_as<::std::size_t>;
};

export module uwvm2.io.reader:impl;
export import :concepts;
// export concrete adapters implementing Reader
```

---

## Reference principles for portable C++ (aligned with uwvm2)

- Prefer freestanding facilities; add hosted features only behind explicit boundaries.
- Validate toolchain support before adopting newly freestanding headers.
- Avoid locale-coupled facilities in core logic.
- Prefer memory mapping for large files and contiguous-range processing.
- Measure; assume no abstraction is zero-cost.

---

## Extended standard library guidance (freestanding-first)

### Current standard status
- Target C++26 for the language/toolchain baseline. Adopt library features only when available and stable across toolchains and in the intended freestanding profile.

### Avoid locale-coupled and non-freestanding headers in core layers
- Do not use `<charconv>`, `<format>`, or other locale-heavy facilities in core code.
- Avoid `<cctype>` for character classification. Provide deterministic, ASCII-focused utilities, or use explicit Unicode processing.

### iostream and stdio policy
- iostream and stdio are not freestanding and are locale-influenced. They also increase binary size significantly.
- If third-party libraries are unavailable, prefer stdio over iostream for hosted code. In core paths, prefer `fast_io`.
- Do not assume consistent performance characteristics across platforms; measure.

### Format-string safety
```cpp
inline void bad(::std::string const& s) {
  // DANGER: format-string vulnerability (do not do this)
  // printf(s.c_str());
}
```

### Avoid `::std::filesystem` in core code
- Design limitations (locale, TOCTOU risks, error reporting) and code size concerns make it unsuitable for core layers. Prefer narrow, explicit APIs.

---

## Integer and character type guidance (expanded)

### Defaults and fixed-width types
- Prefer `::std::size_t` for sizes and indices.
- Prefer `<cstdint>` fixed-width or least-width types (`::std::(u)int_leastXX_t`) for portable ABI and semantics.

### Character types are integers
- Treat `char`, `wchar_t`, `char8_t`, `char16_t`, and `char32_t` as integers. Do not rely on stream behaviors or locale conversions.
- Beware `int8_t*` printing via iostream, which may be treated as a C-string.

### Avoid extended and maximal integer types
- Avoid `__uint128_t` for general portability. Prefer two-limb implementations with `::std::uint_least64_t`.
- Avoid `::std::uintmax_t` and `::std::intmax_t` due to ABI variability.

### Constants
- Use `INTXX_C()` and `UINTXX_C()` macros when defining constants for least-width types.

### Endianness of `wchar_t`
- Do not assume `wchar_t` endianness matches the machine endianness. Normalize explicitly when required.

---

## Floating point and strict aliasing

### Avoid pointer punning; use `::std::bit_cast`
```cpp
#include <bit>
#include <cstdint>
#include <limits>

constexpr float fast_inverse_sqrt(float number) noexcept {
  static_assert(::std::numeric_limits<float>::is_iec559);
  const auto bits = ::std::bit_cast<::std::uint32_t>(number);
  const float y = ::std::bit_cast<float>(UINT32_C(0x5f3759df) - (bits >> 1));
  return y * (1.5f - (number * 0.5f * y * y));
}
```

### FP availability
- Do not assume hardware floating point; soft-float may be present and slow. Verify `<cmath>` behavior and `math_errhandling` on targets.

---

## SIMD guidance

- Prefer compiler vector extensions where available for portability, rather than vendor-specific `<immintrin.h>` intrinsics in core layers.
- Isolate architecture-specific intrinsics behind module partitions and capability checks.

---

## Threads and concurrency

### Avoid spin locks
- Do not use spin locks in preemptive environments; they can stall under scheduler preemption and harm system responsiveness.

### Portability
- Use platform-neutral abstractions or minimal wrappers. Do not assume `<pthread.h>` on Windows or all toolchains.
- Avoid `thread_local` on targets where the ABI support is incomplete (some `cpu-windows-gnu` triples); provide fallbacks.

---

## Additional Windows details

### Importing APIs explicitly
- Import specific Win32/NT APIs with correct calling conventions and linkage. Avoid `extern "C"` declarations that conflict with C++ name mangling and attributes.

### A vs W APIs
- Use W (Unicode) APIs on NT-family systems. Abstract legacy 9x A APIs behind compile-time traits if support is required.

### File descriptors and CRT
- Windows CRT file descriptors wrap HANDLEs. Use `_open_osfhandle` and related APIs when bridging between HANDLEs and CRT descriptors. Keep the types distinct.

### Testing on WINE
- Test Windows builds under WINE when appropriate. Ensure runtime libraries are discoverable via environment configuration.

## Appendices (indices)

- Appendix A: Minimal module/header templates (see examples above).
- Appendix B: Recommended warning flags and sanitizer profiles.
- Appendix C: Platform notes and portability traps.

---

### Appendix A: Minimal module/header templates

```cpp
// foo.cppm
export module uwvm2.utils.foo;
import fast_io;
import uwvm2.utils.container;
import :impl;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "foo.h"
```

```cpp
// foo.h
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include "impl.h"
#endif
```

```cpp
// impl.cppm
export module uwvm2.utils.foo:impl;
export import :bar;
export import :baz;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#include "impl.h"
```

```cpp
// impl.h
#pragma once
#ifndef UWVM_MODULE
# include "bar/impl.h"
# include "baz/impl.h"
#endif
```

---

### Appendix B: Recommended warning flags and sanitizer profiles

Minimum warnings (GCC/Clang):

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wuninitialized -Wunused -Wmisleading-indentation
```

Strongly recommended during development:

```
-fsanitize=address,undefined -fno-omit-frame-pointer -g3
```

For fuzzing harnesses, also consider:

```
-fsanitize=fuzzer
```

Document any required suppressions and keep them narrowly scoped.

---

### Appendix C: Platform notes and portability traps

- Linux/FreeBSD/Darwin: Prefer POSIX APIs behind portability wrappers; avoid implicit locale; use `mmap` where appropriate.
- Windows: Use W (Unicode) APIs on NT; avoid `windows.h` in public headers; wrap HANDLEs with RAII; be mindful of CRT vs Win32 differences.
- WASI/WebAssembly: Respect capability-based IO; provide memory mapping fallbacks; avoid synchronous blocking assumptions.

---

### Appendix J: Build system and toolchain integration

uwvm2 uses xmake as the primary build system. General guidance:

- Configure builds with explicit mode/arch/toolchain flags, e.g.:
  - Development: `xmake f -m debug`
  - Release: `xmake f -m releasedbg`
  - Specific platform/arch: `xmake f -p linux -a x86_64`
- Use the repository’s provided xmake rules and options under `xmake/`.
- Keep module file structure consistent; the build graph assumes the `.cppm`/`.h` mirroring rules.
- For cross builds, ensure the toolchain provides freestanding-friendly headers when targeting freestanding layers.

#### Static analysis
- The repository includes a clang-tidy integration under `xmake/plugins/clang-tidy/`. Run it locally on changed files before submitting.
- Keep suppressions minimal and justified in code comments next to the site or via `.clang-tidy` configuration.

#### Warning policy
- Treat warnings as errors in CI for core code paths. Locally, keep a warning-clean build with the recommended flags.

---

### Appendix K: Module bridge macros and header policy

- `UWVM_MODULE` is a guard macro used to bridge module interface units and headers. It must only be defined by the bridge block in `.cppm` files and should never be defined globally by build flags.
- `UWVM_MODULE_EXPORT` wraps `export` where needed for compatibility. In `.cppm` files, define it as `export` if not already defined.
- Headers should start with `#pragma once` and avoid global `using namespace` directives.
- Do not place `using namespace std;` in any header. Avoid using-directives in implementation unless scoped and justified.

Example bridge pattern:
```cpp
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
```

Header-side pattern (so the same header can be included by `.cppm` with `export` and by non-module builds):
```cpp
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::example
{
    // exported declarations...
}
```

---

### Appendix L: Header and file prologues

Most core/runtime files use a standardized UWVM2 prologue (license/copyright + `@author`/`@version` banner and an ASCII mark). When adding a new file under `src/uwvm2/runtime/` (or adjacent core directories), copy the prologue from a nearby file and keep it intact.

Prefer documenting purpose and constraints as close to the exported surface as practical:
- a short `@brief` Doxygen comment for the primary namespace/type/function,
- explicit constraints (freestanding-only, exceptions, thread-safety) near the API, not buried in file boilerplate.

---

### Appendix M: Documentation and Doxygen

- Use Doxygen-style comments for public APIs and module exports.
- Group related APIs using `@defgroup`/`@ingroup`.
- Keep parameter and return descriptions specific, with preconditions and postconditions where relevant.
- The repository contains a Doxygen configuration (`doxyfile`) and CSS (`documents/doxygen/customdoxygen.css`). Ensure added documentation renders cleanly.

---

### Appendix N: Error codes and domains

- Define error domains as small, numeric spaces with stable meanings.
- Reserve ranges for subsystems (parser, IO, memory, platform).
- Avoid overlapping codes across domains. Map OS errors explicitly.

Sketch:
```cpp
enum class parse_error : int {
  ok = 0,
  invalid_magic = 1,
  truncated = 2,
  // ...
};
```

---

### Appendix O: Include order within categories

Within each category, order includes alphabetically by fully qualified path or module name. Do not interleave categories.

Rationale: This stabilizes diffs and avoids order-dependent surprises.

---

### Appendix P: Third-party code policy

- Vendor third-party code under `third-parties/` with license files intact.
- Avoid modifying vendored code; if necessary, keep changes minimal and well-documented with clear diffs.
- Wrap third-party headers behind uwvm2 modules/headers to insulate the rest of the codebase from direct dependencies.

---

### Appendix Q: Global/static initialization

- Avoid global constructors in core layers. Prefer explicit initialization during startup phases.
- If unavoidable, ensure initialization order is explicit and does not depend on cross-translation-unit state.

---

### Appendix R: Deterministic IO and locales

- Set locales explicitly in hosted tooling where necessary; default to a stable, C-like locale for deterministic behavior.
- Do not rely on environment locale in core code.

---

### Appendix S: Naming conventions

- Module names: lowercase dotted hierarchy (e.g., `uwvm2.parser.wasm.base`).
- Partitions: short, meaningful names (e.g., `:impl`, `:algo`, `:adapters`, `:detail`). Avoid `:internal` unless the intent is clear.
- Types: `PascalCase` for public types; `snake_case` for private helper types is acceptable within small scopes.
- Functions: `snake_case`, consistent within a module. Prefer consistency over style debates.

---

### Appendix T: Prohibited patterns

- `using namespace` in headers.
- C++ exceptions in freestanding/core layers.
- Locale-coupled behavior in core logic.
- Mixing module imports with preprocessor-heavy code in the same `.cpp`.
- Managing OS resources with generic smart pointers instead of domain RAII types.

### Appendix D: Error handling taxonomy

- OS/transport errors: map to numeric codes (`errno`, Win32) and propagate without throwing in core layers.
- Semantic validation errors: use explicit error domains and structured results.
- Invariants: assert in debug builds; fail fast when violated in release if recovery is not meaningful.

Example result type sketch:
```cpp
// Pseudocode; use a small, trivially copyable status type
struct status {
  int domain{}; // narrow domain id
  int code{};   // code within domain
};

template <class T>
struct result {
  T value{};
  status st{};
  explicit operator bool() const noexcept { return st.domain == 0 && st.code == 0; }
};
```

---

### Appendix E: Determinism guidelines

- Avoid reliance on environment variables and process-global locale/state.
- Make randomness pluggable and seedable; avoid hidden global RNGs.
- Ensure stable iteration orders unless nondeterminism is explicitly required and documented.

---

### Appendix F: Performance methodology

- Benchmark with realistic inputs and cold/warm cache scenarios.
- Isolate IO from compute to measure correctly; avoid mixing concerns in microbenchmarks.
- Use CPU performance counters where available; guard platform-specific tooling behind scripts.

---

### Appendix G: Macro usage policy

- Minimize macros; prefer `constexpr` and templates.
- Use `#define` only for platform feature detection, small configuration toggles, or compile-time switches that cannot be expressed otherwise.
- Do not leak macros via public headers; undefine after the smallest scope when necessary.

---

### Appendix H: ABI/API stability

- Keep exported module interfaces stable within a release series.
- Avoid inline ABI-breaking changes; gate new behavior behind versioned modules or feature flags.
- Document any intentional ABI changes in release notes and provide migration steps.

---

### Appendix I: Test and fuzz harness patterns

- Unit tests in `test/` (`.cc`) should avoid platform-specific assumptions; gate when unavoidable.
- Fuzzers target parser and binary format boundaries; seed corpora with minimal valid/invalid samples; enforce timeouts and memory limits.

## uwvm2 memory models (object layer)

This document provides a high-level overview of the host memory models used by
the `uwvm2::object::memory` subsystem to implement WebAssembly linear memory.

It explains how logical WebAssembly pages, platform pages and host-side linear
memory backends relate to each other, and how `native_memory_t` selects an
appropriate implementation for the current platform.

---

### 1. Logical WebAssembly page model (`wasm_page`)

At the specification level, uwvm2 models memory in terms of WebAssembly pages:

- Page granularity is **64 KiB**.
- Both **wasm32** and **wasm64** variants are supported.
- Types and helpers are defined under
  `uwvm2::object::memory::wasm_page`.

These types capture the *logical* view that the WebAssembly module sees:
`memory.grow` / `memory.size` operate in units of pages, independent of the
underlying host operating system.

---

### 2. Platform page model (`platform_page`)

The platform page model describes the *physical* page granularity provided by
the host operating system:

- Exposed via `uwvm2::object::memory::platform_page`.
- Encapsulates platform-specific queries such as the OS page size.
- Used by the linear memory backends to align allocations and protection
  boundaries with the host page size.

This layer decouples the WebAssembly page abstraction from the concrete host
page size and APIs (for example `VirtualAlloc`, `mmap`, and related calls).

---

### 3. Host linear memory models (`linear::native_memory_t`)

uwvm2 provides **three** concrete host-side models for implementing the
WebAssembly linear address space. They live under
`uwvm2::object::memory::linear` and are selected via the `native_memory_t`
alias in `linear/native.h`.

#### 3.1 mmap-based linear memory (`mmap_memory_t`)

- Enabled when `UWVM_SUPPORT_MMAP` is defined.
- Uses the operating system's virtual memory facilities (for example,
  `VirtualAlloc` on Windows and `mmap` on POSIX) to reserve a large contiguous
  address range for the linear memory.
- Grows memory by committing additional pages inside the reserved range instead
  of reallocating and copying.
- Provides page-protection-based trapping semantics and is fully
  multi-thread capable.
- Exposed as `uwvm2::object::memory::linear::mmap_memory_t` and selected via:

  ```cpp
  using native_memory_t = mmap_memory_t;
  ```

#### 3.2 Multi-threaded allocator-based linear memory (`allocator_memory_t`)

This model is used on platforms that **do not** support `mmap` but **do**
support multi-threading and C++20 atomic wait/notify primitives.

- Enabled when `UWVM_USE_MULTITHREAD_ALLOCATOR` is defined and the standard
  library provides `std::atomic::wait` / `notify_*`.
- Backed by a generic aligned allocator instead of virtual memory.
- Implements a careful synchronization protocol for `memory.grow` and
  in-flight memory operations:
  - `growing_flag_guard_t` provides an exclusive region for `grow`.
  - `memory_operation_guard_t` coordinates active readers/writers and prevents
    races with relocation during growth.
- Offers multi-thread-safe linear memory on embedded or constrained platforms
  where virtual memory reservation is unavailable.
- Exposed as `uwvm2::object::memory::linear::allocator_memory_t` and selected
  via:

  ```cpp
  using native_memory_t = allocator_memory_t;
  ```

#### 3.3 Single-thread allocator-based linear memory
(`single_thread_allocator_memory_t`)

This is the fallback model for platforms without `mmap` and without the need
for multi-threaded access to the WebAssembly linear memory.

- Uses the same aligned allocator strategy as the multi-threaded allocator
  model, but omits atomic counters and locks.
- Intended for strictly single-threaded runtimes where `memory.grow` and all
  memory operations are serialized by design.
- Exposed as
  `uwvm2::object::memory::linear::single_thread_allocator_memory_t` and
  selected via:

  ```cpp
  using native_memory_t = single_thread_allocator_memory_t;
  ```

At compile time exactly **one** of these three implementations becomes the
`native_memory_t` alias, providing a uniform interface to the rest of the
object subsystem while allowing the backend to be tailored to the host.

---

### 4. Multiple memories (`multiple_native_memory_t`)

The `multiple` namespace provides container support for managing more than one
linear memory instance:

- `uwvm2::object::memory::multiple::multiple_native_memory_t` is a
  vector-like container over `linear::native_memory_t`.
- It is used to represent modules or runtimes that expose multiple distinct
  WebAssembly memories while reusing the same underlying host memory model.

This keeps the semantics of each individual memory consistent with the
selected host model while allowing the object layer to scale to multiple
memories when required by the WebAssembly module.

---

### 5. Command-line control (`uwvm`): strict `memory.grow`

The object-layer memory backends provide both *fail-fast* (terminate on
allocation failure) and *strict* growth
paths. The `uwvm` CLI exposes a switch that selects the strict policy for
WebAssembly `memory.grow`.

#### 5.1 Parameter definition

The option is defined as
`uwvm2::uwvm::cmdline::params::wasm_memory_grow_strict` in:

- `src/uwvm2/uwvm/cmdline/params/wasm_memory_grow_strict.h`

It registers:

- Primary name: `--wasm-memory-grow-strict`
- Alias: `-Wmemstrict`

#### 5.2 Semantics

When enabled, the runtime sets the global flag
`uwvm2::object::memory::flags::grow_strict` (see
`src/uwvm2/object/memory/flags/storage.h`), and the engine may choose a
non-trapping growth path (for example, the `linear::*::grow_strictly(...)`
APIs) so that a growth failure becomes a normal `memory.grow` failure result
instead of a fatal trap or process termination.

This is most meaningful on systems where overcommit is disabled (or where the
platform provides a reliable commit/protection failure signal at grow time). On
overcommit-enabled systems, `memory.grow` success still cannot guarantee that
future writes will not trigger an out-of-memory kill; strict mode only affects
how immediate growth failures are reported.

#### 5.3 Standards alignment

uwvm2's two `memory.grow` modes exist to preserve WebAssembly semantics across
host operating systems that expose very different virtual-memory admission
models.

At the WebAssembly specification level:

- `memory.grow` returns the **previous** memory size on success and `-1` on
  failure.
- Growth **must fail** if it would exceed the memory's declared maximum.
- Growth **may also fail for other reasons**, because the instruction is
  explicitly resource-dependent and therefore non-deterministic from the point
  of view of the embedder.
- On success, the newly added bytes are required to be zero-initialized.
- None of this changes ordinary load/store semantics: out-of-bounds memory
  accesses still trap in the normal WebAssembly way.

In other words, WebAssembly does **not** require an engine to guarantee that a
successful `memory.grow` implies future host-level physical availability in all
environments. It only constrains the observable abstract result:

- if growth succeeds, the module sees the old page count;
- if growth fails, the module sees `-1`;
- exceeding the declared maximum is never allowed to succeed.

uwvm2 follows this contract in both modes. The difference between the two modes
is **not** the Wasm-level memory model, but **when and how host allocation
failure is surfaced**:

- In **strict** mode, the engine tries to translate an immediate host-side
  failure into an ordinary WebAssembly `memory.grow` failure result (`-1`).
- In **fail-fast** mode, the engine prefers a simpler and more aggressive host
  growth path. This is useful on systems where optimistic virtual-memory
  admission is normal and where "success now, kill later on page touch" is an
  unavoidable part of the host's behavior.

This distinction is important because WebAssembly's abstract semantics sit on
top of host kernels that do not agree on when memory pressure should be
detected.

#### 5.4 Why uwvm2 needs two `memory.grow` modes

There is no single host policy that is simultaneously ideal for:

- Linux systems with configurable overcommit,
- Windows systems with explicit reserve/commit accounting,
- BSD-family systems that do not expose Linux's standardized three-mode
  contract,
- embedded or allocator-backed environments without full virtual-memory
  reservation semantics.

As a result, uwvm2 intentionally separates two concerns:

1. **WebAssembly correctness**
   `memory.grow` must never exceed the Wasm-declared maximum or the engine's
   architectural limit.

2. **Host admission policy**
   If the host refuses additional virtual memory, should uwvm2 convert that into
   a normal Wasm `-1`, or should it treat the failure as fatal?

The answer depends on the host:

- On some hosts, immediate failure reporting is reliable and useful.
- On others, an immediate "success" still does not promise future page
  availability, so a recoverable Wasm failure result can only ever be
  best-effort.

Providing both modes lets uwvm2 remain portable without pretending that all
operating systems offer the same strength of resource guarantees.

#### 5.5 Host-policy adaptation

##### Linux: three explicit overcommit regimes

Linux is the clearest example of why `memory.grow` policy cannot be reduced to
a single universal rule. The kernel exposes three overcommit modes through
`vm.overcommit_memory`:

- `0`: heuristic overcommit (default),
- `1`: always overcommit,
- `2`: do not overcommit; enforce a commit limit based on swap and a configured
  RAM allowance.

This matters directly to WebAssembly engines:

- Under **mode 1**, a large anonymous reservation/commit can be admitted even
  when future physical backing is not realistically available. This is the
  classical sparse-array / optimistic-allocation environment.
- Under **mode 0**, the kernel performs heuristic refusal of obvious excess, but
  still allows overcommit in many ordinary cases.
- Under **mode 2**, the system gives the strongest admission-time signal. This
  is the Linux configuration in which translating host grow refusal into Wasm
  `-1` is most meaningful.

For uwvm2, the engineering consequence is straightforward:

- **strict mode** is the best semantic match for Linux mode `2`, and is still a
  reasonable best-effort policy in modes `0` and `1`;
- **fail-fast mode** accepts that on optimistic Linux configurations, immediate
  success may still be followed by later host OOM termination when pages are
  actually touched.

##### Windows: reserve/commit rather than Linux-style overcommit selection

Windows does not present user space with the Linux `0/1/2` overcommit knob.
Instead, its virtual-memory API separates **reservation** from **commit**:

- `MEM_RESERVE` reserves address space without allocating physical storage or
  paging-file backing;
- `MEM_COMMIT` charges commit against the system's available backing and
  guarantees zero-filled contents on first access, while still allocating actual
  physical pages lazily when touched.

For a Wasm runtime, this is a different model from Linux mode `1`:

- admission is not governed by a public "always overcommit" policy switch;
- the host exposes a much more explicit reserve/commit distinction;
- commit failure is generally a meaningful signal that can be mapped to
  `memory.grow == -1` in strict mode.

That said, even Windows does not turn `memory.grow` into a universal future
execution guarantee in the strong mathematical sense. It provides a more
structured admission model, but uwvm2 still benefits from keeping the same two
runtime policies for consistency across backends and platforms.

##### BSD-family systems: host-defined admission, not Linux's 0/1/2 contract

BSD-family kernels are precisely why uwvm2 documentation should avoid treating
Linux overcommit policy as if it were universal.

- **FreeBSD** exposes its own `vm.overcommit` sysctl, but its semantics are not
  Linux's `0/1/2` API. The system always accounts swap reservation and can be
  configured to fail allocation when reservation exceeds available backing.
- **OpenBSD** and **NetBSD** document ordinary `mmap(..., MAP_ANON, ...)`
  failure with `ENOMEM` when insufficient memory is available, but they do not
  expose the same standardized three-mode public contract that Linux does.

From a portable runtime-design perspective, the important conclusion is:

- BSD-family environments should be treated as **host-policy-defined**
  environments, not as instances of a single Linux-like overcommit taxonomy.

In practical engineering discussions, this often puts BSD-like systems into the
same bucket as "effectively overcommit-permissive or host-defined" hosts:
portable user space cannot assume a Linux-mode-2-style admission contract
across the family.

That is why uwvm2 does **not** hard-code a single memory-growth policy around a
particular OS family. Instead, it keeps two explicit engine policies:

- one that prefers recoverable Wasm failure reporting when the host offers a
  usable immediate failure signal;
- one that prefers simpler fail-fast behavior when the host's VM policy makes a
  recoverable result either impossible or operationally misleading.

#### 5.6 What the two modes do and do not change

The two `memory.grow` modes change **reporting strategy**, not the abstract
memory model.

They do change:

- whether immediate host growth refusal is surfaced as Wasm `-1` or treated as
  fatal,
- how aggressively the runtime relies on the host's growth API,
- how closely `memory.grow` failure reporting tracks host admission-time
  resource checks.

They do **not** change:

- the WebAssembly page size,
- maximum-size enforcement,
- zero-initialization of newly grown memory,
- out-of-bounds trap behavior for ordinary memory accesses,
- the backend-neutral object-layer API exposed to the rest of uwvm2.

This separation is intentional. `memory.grow` is not a hot-path arithmetic
instruction; it is a heavy operation whose observable behavior depends in part
on the host kernel. uwvm2 therefore optimizes this area for **portability and
semantic clarity**, not for micro-optimizing a path that is cold by design.

#### 5.7 Usage

Example:

- `uwvm --wasm-memory-grow-strict ...`
- `uwvm -Wmemstrict ...`

---

### 6. References

- WebAssembly Core Specification, execution semantics for `memory.grow` and
  `growmem`:
  - <https://webassembly.github.io/spec/core/exec/instructions.html>
  - <https://webassembly.github.io/spec/core/exec/modules.html>
- Linux kernel documentation, overcommit accounting:
  - <https://docs.kernel.org/mm/overcommit-accounting.html>
- Microsoft documentation for Windows virtual memory reserve/commit semantics:
  - <https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2>
  - <https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-memory_basic_information>
- FreeBSD tuning manual (`vm.overcommit`):
  - <https://man.freebsd.org/cgi/man.cgi?tuning>
- OpenBSD and NetBSD `mmap(2)` manual pages:
  - <https://man.openbsd.org/OpenBSD-7.1/mmap.2>
  - <https://man.netbsd.org/mmap.2>

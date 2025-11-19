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


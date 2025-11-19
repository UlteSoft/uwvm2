# WebAssembly System Interface Preview 1 (WASI P1)

This directory contains UWVM2's implementation of the WebAssembly System Interface Preview 1 (WASI P1). It provides the host-side ABI that exposes
files, directories, clocks, and other system resources to WebAssembly modules targeting:

- `wasm32-wasip1`
- `wasm64-wasip1`

The implementation is integrated into the `uwvm2::imported` subsystem and is shared by both native hosts and self-hosted UWVM2 builds (UWVM2 compiled
as a WASI binary).

## Design goals

The WASI P1 layer in UWVM2 is designed with the following goals:

- **Spec alignment.** Follow the official WASI Preview 1 specification for ABI layout, pointer sizes, error codes, and capability-based rights.
- **Cross-platform behavior.** Reuse the same portability layer as the core VM so that the same WASI P1 module can run on a wide range of hosts.
- **Safety and robustness.** Enforce strict bounds checks, UTF-8 validation (where enabled), and capability checks instead of trusting guest input.
- **Predictable concurrency.** Coordinate access to linear memory and host state so that multi-threaded execution does not introduce undefined behavior.

## Cross-platform host support

UWVM2's WASI P1 implementation is built on top of the same platform abstraction and `fast_io` back-end used by the virtual machine itself. As a result,
it is **designed to be portable** across the same family of targets as UWVM2:

- **DOS-family environments.** Classic DOS-style targets, via the portable file-system and directory abstractions.
- **POSIX-family systems.** Linux, *BSD, macOS and other POSIX-like systems.
- **Windows family.** From early Windows 9x/NT up to modern Windows 10/11.

For each host, the WASI layer maps WASI P1 operations to the native API surface that is available via `fast_io` and the underlying C++ runtime. Where a
particular operation cannot be implemented exactly on a given host, the layer returns the closest appropriate WASI error code rather than silently
degrading behavior.

## Environment model

At the heart of the implementation is the `wasip1_environment<memory_type>` type in `environment/environment.h`:

- **Single linear memory.** WASI P1 (like the WebAssembly 1.0 MVP) assumes at most one linear memory. The environment therefore keeps a pointer to
  `memory[0]` only.
- **Arguments and environment variables.** The environment stores `argv` and `envs` as vectors of `u8string_view`, allowing the embedder either to use
  the VM's environment by default or to inject a custom configuration.
- **File-descriptor storage.** The `fd_storage` member holds the WASI P1 file-descriptor table, including rights, types (file, directory, etc.), and
  per-descriptor state such as directory stacks.
- **Pre-opened directories.** A `mount_dir_roots` collection represents directories that have been preloaded into the environment, which are then
  transferred into the file-descriptor manager.
- **Tracing and validation toggles.** Flags such as `trace_wasip1_call` and `disable_utf8_check` let the embedder enable detailed logging or relax
  certain checks when debugging.

This environment type is intended to be instantiated per WebAssembly instance. The embedding runtime is responsible for deciding how WASI calls on the
same environment are scheduled across threads.

## Memory model, safety, and concurrency

The WASI P1 memory helpers live in `memory/allocator.h` and operate on UWVM2's linear memory back-ends. They embody several important properties:

- **Concurrency safety.** Access to the underlying linear memory is guarded by a `memory_operation_guard_t` (via `lock_memory` and related helpers),
  which coordinates reads/writes with `memory.grow` operations using atomic flags and, where available, `std::atomic_wait`. This prevents races between
  concurrent WASI calls and memory growth.
- **Bounds double-checking.** Helpers such as `check_memory_bounds*` and `check_memory_bounds_wasm32/wasm64` verify that a given byte range fits inside
  the current memory, taking into account potential overflows when converting between 32-bit/64-bit WASI pointers and `size_t`.
- **Well-defined failure behavior.** If a bounds violation is detected, the helpers call `output_memory_error_and_terminate` with detailed diagnostic
  information instead of allowing undefined behavior.
- **Endianness handling.** All WASI-visible integer types are encoded as little-endian in memory, with explicit byte-swapping on big-endian or
  non-standard-char-size platforms.

These helpers are used by the individual WASI P1 functions to read/write arguments and results from/to the guest's linear memory for both
`wasm32-wasip1` and `wasm64-wasip1` variants.

## File-descriptor and path handling

The file-descriptor manager in `fd_manager` provides the core machinery for implementing WASI P1's capability-based I/O model:

- **Capability-based rights.** Each entry in the descriptor table tracks base and inheriting rights. The WASI P1 functions check these rights before
  performing operations such as `path_open`, `fd_read`, or `fd_renumber`, returning errors like `enotcapable` when the caller lacks the required
  capabilities.
- **Typed descriptors.** Descriptors differentiate between files, directories, and other resources. This allows the implementation to reject invalid
  operations early (for example, using a file descriptor where a directory is required).
- **Native integration via fast_io.** The manager uses `fast_io` to open, read, write, and enumerate host files and directories, leveraging
  platform-specific features while presenting a uniform WASI P1 surface.

The test suite under `test/0008.imported/wasi/wasip1/func` exercises these behaviors for both positive and negative cases (for example, invalid
descriptors, missing rights, invalid UTF-8, and out-of-range offsets).

## Thread safety and partial atomicity

Thread safety and atomicity are addressed at several layers:

- **Linear memory operations.** Memory reads and writes that go through the WASI helpers are synchronized with growth operations via atomic guards and,
  when available, `std::atomic_wait`-based blocking. This ensures that a WASI call either observes a consistent view of memory or terminates with a
  controlled error when invariants are violated.
- **Host-side operations.** Many WASI P1 operations correspond to single host system calls (for example, certain file opens, renames, or metadata
  queries). Where the underlying operating system provides atomic guarantees, the WASI implementation preserves them; where it does not, the closest
  well-defined behavior is chosen and signaled via standard WASI error codes.

The environment itself is intentionally simple and is expected to be used behind the synchronization policies of the embedding runtime. This allows
applications to choose between strictly serialized WASI calls per instance, or more advanced parallel scheduling strategies.

## Testing and validation

In addition to compile-time checks (such as `static_assert`-based validation of the `wasip1_memory` concept against specific memory back-ends), the
implementation is covered by targeted tests in `test/0008.imported/wasi/wasip1/func`. These tests validate both the ABI surface (pointer sizes,
argument layouts) and the semantic behavior (rights checks, error codes, and interaction with the host file system).

Together, these components provide a portable, spec-aligned, and safety-conscious WASI Preview 1 implementation that can be reused across the wide
range of platforms supported by UWVM2.

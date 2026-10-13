# Version output manual dates and ROS marker (2026-10-09)

Both products retain the existing 1.0 / 1.1 entries and now also print:

- WebAssembly Release 2.0 (Draft 2025-01-30)
- WebAssembly Release 3.0 (Draft 2024-09-21)
- WebAssembly Release 2.0 + Threads (Draft 2023-10-10)

The dates were extracted from page 1 of the corresponding PDFs in
`../wasm-spec/WebAssembly Specifications`. These are the repository-linked
manual snapshots, not the revision date of today's specification website.
The version output previously enumerated only the binfmt parser/storage hook
tuple. The additional display entries do not change parser registration or
runtime feature policy. ROS now adds a bright-purple `[ROS]` after its version
number through `diagnostic_color`; disabling color preserves the marker text.

Actual version callback output was compiled and executed for both products:
Clang 23, C++26, O0, warnings as errors, real fast_io lockable native sink,
real color helper and version formatter. Both declaration forms passed four
cases: explicit color on/off, empty NO_COLOR, and explicit color override.
macOS: 16/16 checks; observed owned-process RSS peak 475,283,456 bytes.
Linux: 16/16 checks in the existing 64 GiB / swap-zero cgroup; owned RSS peak
547,700,736 bytes. All owned PIDFDs retired; OOM counters did not change.
Both products' existing fast_io CPO arity checks passed on both hosts.

The fixture stubs parser feature-pack and CLI registry declarations; it is a
callback check, not a complete VM/CLI link or a named C++ modules build.
macOS SDK/linker setup and Linux loader/system-include failures were retained
under `/tmp/uwvm2-version-manual-20261009` and the matching Linux directory;
only the final successful checks qualify this change.

# Changelog

This document tracks externally visible release history for `uwvm2`.

Each release entry should record:

- release version and channel
- release date and lifecycle status
- distribution baseline commit
- beta-phase fixes
- general-availability or post-release fixes
- maintenance support window
- deprecation notice dates and removal targets

## Entry Conventions

- `Added`: user-visible capabilities first introduced in the release.
- `Changed`: non-breaking behavioral, packaging, or documentation updates.
- `Fixed`: defects resolved in the release cut itself.
- `Beta Fixes`: fixes made while the release remains in beta.
- `Release Fixes`: fixes made after general availability.
- `Deprecated`: items officially marked as deprecated, including notice dates and removal targets where known.

## V2.0.0.1

- Release channel: `Beta`
- Release status: `Initial public beta`
- Release date: `2026-03-30`
- Planned general availability: `2026-06-30`
- Distribution baseline commit: `TBD` when the release artifact or tag is finalized
- Current source baseline reference: `95b1836d31af291feb8ee08feb3d8eedfecfded8`
- Source baseline timestamp: `2026-03-30T14:45:08+08:00`
- Maintenance support window: `TBD`
- Deprecated notice date: `None`
- Removal target: `None`

### Added

- Initial public beta release of the UWVM2 2.x line.
- Cross-platform WebAssembly runtime foundations, including the `u2` interpreter, parser and validation pipeline, WASI Preview 1 host integration, plugin and preload module support, and multiple linear-memory backends.

### Changed

- No prior public 2.x release exists, so no release-to-release functional delta applies yet.
- Build configuration currently force-disables JIT support because no production-ready JIT backend is available in this release line.
- Commit reference: `95b1836d31af291feb8ee08feb3d8eedfecfded8`
- `xmake` JIT configuration is restricted to `none`, and JIT is therefore unavailable for `V2.0.0.1`.

### Fixed

- None recorded for the initial beta distribution cut.

### Beta Fixes

- Removed unintended LLVM JIT environment probing during `xmake f` configuration when JIT support is disabled.
- The hidden `llvm-jit-env` configuration path has been removed so `llvm-config` is no longer queried for `V2.0.0.1` builds.
- Renamed the build-toolchain option from `--use-llvm` to `--use-llvm-compiler` to make it explicit that the flag selects the LLVM/Clang compiler toolchain rather than enabling JIT.

### Release Fixes

- Not applicable until general availability.

### Deprecated

- No deprecations have been formally announced for this release line at this time.

### Notes

- Update `Distribution baseline commit` to the exact tagged commit used to produce release artifacts when the beta package is cut or re-cut.
- Record every externally relevant beta hotfix and post-release maintenance fix under this version entry until a newer public version supersedes it.

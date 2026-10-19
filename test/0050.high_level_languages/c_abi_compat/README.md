# C/C++ libc argv and environ compatibility

These are real C17 and C++20 WASI Preview 1 programs compiled with official
**wasi-sdk 20.0 / Clang 16.0.0**. The C++ program also copies the arguments
through `std::vector<std::string>`. Assertions remain enabled.

Both programs check `argv[argc] == NULL`, UTF-8 and empty arguments, `getenv`
including empty/UTF-8 values, and traversal of `environ` to its trailing NULL.
The matrix uses 0/1/3 user arguments (argc 1/2/4) and 0/1/3 environment entries.

WASI writes an exact-count pointer table; it does not add a pointer after that
table. The [WASI Preview 1 contract](https://github.com/WebAssembly/WASI/blob/snapshot-01/phases/snapshot/witx/wasi_snapshot_preview1.witx)
defines its size through the corresponding sizes call. C's additional NULL slot
is provided by guest libc: wasi-libc's [argv startup](https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/sources/__main_void.c)
and [environment initialization](https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/sources/__wasilibc_initialize_environ.c)
allocate count + 1 zeroed pointers. This does not require a language-specific VM
mode. Individual argument/environment strings remain NUL-terminated.

Run provisioning/builds/executions only inside the existing SSH Linux 64 GiB
cgroup. With the pinned SDK already installed:

```sh
export WASI_SDK_PATH=/tmp/high-level-20261017/csharp/wasi-sdk-20.0
bash build.sh /tmp/high-level-20261017/c_abi_compat/artifacts
python3 run.py --arena /tmp/high-level-20261017 \
  --artifacts /tmp/high-level-20261017/c_abi_compat/artifacts \
  --wasmtime /home/macromodel/.wasmtime/bin/wasmtime \
  --out /tmp/high-level-20261017/c_abi_compat/results \
  --expected-cgroup /system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope
```

On 2026-10-17, all **72 cases passed**: 18 guest configurations each on
uwvm2, uwvm2-stable, uwvm2-ros, and Wasmtime 42.0.1. `results.json` preserves
commands, binary/module hashes, exit status, and stdout for each case. Cgroup
memory events did not change during this matrix. Generated binaries stay in
the task-owned scratch directory.

The original `results.json` is pre-reboot evidence. Post-reboot reports under
`recovery/` identify the rebuilt modules and replacement VM binaries separately.
Use repeatable `--runner` options to qualify available engines without starting
an unavailable product; omitting them runs all four engines.

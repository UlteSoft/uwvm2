This delivery preserves the actual FullOz producer source, not a rewritten replacement. `evidence-sources/executed-layout/test/0050.high_level_languages/python/debug_oz_build.py` has SHA256 `f3b10b16d5df0972b925b5d612c74f2fa6358f5633ad56b52ba6ebffd746bf03`, matching the final build receipt. The same file also exists at the ordinary suite path `python/debug_oz_build.py` in the ROS working tree. Its reference helper SHA is `24a58c47a17feb25cbb4f4a9183a94f14520c59282b5ccfff53530fbced29d53`.

The nested layout is intentional: the real scripts resolve fixture peers beside `__file__` and common `inspect_wasm.py`/`owned_process.py` one directory above. Keep this layout intact. `capture-manifest.json` records direct imports, receipt links, dependency scope and failed-attempt source hashes; `SHA256SUMS` binds the delivered files. These files were copied and hash-compared without running a producer/compiler/VM.

The final `f3b10b16…` invocation actually used `--resume-built`. Configure and make had already succeeded under the preserved `d3d83eeb…` attempt, which failed only in compiler-log metadata parsing. The final helper authenticated the exact old argv and successful closed/retired lifecycle, reused those stages, audited252 effective -Oz compilation commands, linked and inspected the real module. Both actual scripts are supplied unchanged. The fresh-work recipe below uses the current script's existing fresh branch, but **that recipe was not newly executed for this source delivery**.

**Existing native/frozen dependencies**

The executed original base is `/tmp/high-level-20261017/python/work-r4`, whose final receipt SHA is `4eb71c6a10490a270b895cf2d66b542ab70fd2511d7dfae5d24e81065c020816`. It supplies:

| Input | Actual location / identity |
| --- | --- |
| Native CPython3.14.8 | `sources/python/Python-3.14.8/cross-build/x86_64-pc-linux-gnu/python`, SHA `f36bef8c90257efac58a0ebc1e5c0f5231ab0f5c00246fdac9c014de123d837c` |
| Qualified custom frozen table | `frozen-semantics/hll_frozen.h`, SHA `2bfb80b80b4651b152926b9e5bbf7de80545a87ff5c00bd6e0559f23f191553e` |
| Frozen source/bytecode receipt | `frozen-semantics/frozen-assets.json`, SHA `26f570f713f66be85af2000f8777f7993f063485737fba044c1c1d4304f92132` |
| SDK24 clang | `sources/sdk/wasi-sdk-24.0-x86_64-linux/bin/clang`, SHA `64280d1cf3e55fe73bffe55deed0e61936c84a1b538a5435e50ad8e028eb2e3b` |

The header and frozen manifest are included under `evidence-sources/dependencies/original-frozen-semantics`. Generated native/Oz Makefile, pyconfig and module setup/config files are included under `dependencies/current-arena`. The final FullOz generated config.c SHA is `1664efaae522119f80d2a5b0a38579482846c976a53b324afabcab7de0d64df3`, identical to the full O2 table with71 builtins. The Oz helper symlinks the original native build tree as its build-Python dependency and copies the exact qualified frozen header; it does not rebuild native CPython or regenerate that header.

The original native is an O2 host build, while the new interpreter C sources and embedding launcher compile -Oz -g. Python code objects retain optimize0/assertions. CPython feature/GC/assertion macro configuration is unchanged. This does not qualify the original O2 module's DBG behavior. SDK archive pin covers the complete toolchain/sysroot; `dependencies/fullOz-dependency-capture-r1.json` additionally records current libc, emulation and clang-rt archives. It is explicitly a post-run read-only capture, not a retrofit into the old build receipt.

**Fresh-directory reproduction recipe, not executed in this delivery**

Run only after the parent releases a build window, inside the existing SSH Linux Docker cgroup64GiB/swap0. No local workstation build or new cgroup is allowed. The existing Linux arena has the official static archives; if absent, reacquire the exact URLs/hashes from capture-manifest.json inside that same environment. CPython's source SHA is official; SDK24's captured GitHub asset has no published digest, so its listed SHA is this run's observed reproducibility pin.

```sh
ARENA=/tmp/high-level-20261017
SUITE=$ARENA/uwvm2-ros/test/0050.high_level_languages
CAPTURE=$SUITE/python/debug-producer/evidence-sources
PRODUCER=$CAPTURE/executed-layout/test/0050.high_level_languages/python
WASMTIME=/home/macromodel/.wasmtime/bin/wasmtime
INPUTS=$ARENA/python/rebuild-inputs-r1
BASE=$ARENA/python/rebuild-base-r1
BASE_REFS=$ARENA/python/rebuild-base-refs-r1
BASE_CASES=$ARENA/python/rebuild-base-manifests-r1
OZ=$ARENA/python/rebuild-full-Oz-r1
OZ_REFS=$ARENA/python/rebuild-full-Oz-refs-r1
LOGS=$ARENA/python/rebuild-launchers-r1

# All target directories must be fresh and task-owned.
mkdir "$INPUTS" "$LOGS"
ln "$ARENA/python/static-inputs/Python-3.14.8.tar.xz" "$INPUTS/Python-3.14.8.tar.xz"
ln "$ARENA/python/static-inputs/wasi-sdk-24.0-x86_64-linux.tar.gz" "$INPUTS/wasi-sdk-24.0-x86_64-linux.tar.gz"
cp "$CAPTURE/dependencies/downloads/wasi-sdk-release.json" "$INPUTS/wasi-sdk-release.json"
(cd "$INPUTS" && sha256sum --check "$CAPTURE/dependencies/downloads/SHA256SUMS")

run_owned() {
    _python_rebuild_log=$1
    shift
    taskset -c 4-7 /usr/bin/python3 -c \
      'import sys; sys.path.insert(0,sys.argv[1]); from linux_control import invoke; invoke(sys.argv[3:],sys.argv[2],timeout=1800)' \
      "$PRODUCER" "$_python_rebuild_log" "$@"
}

run_owned "$LOGS/base-build" /usr/bin/python3 "$PRODUCER/build.py" \
    --work "$BASE" --input-dir "$INPUTS" --wasmtime "$WASMTIME" --jobs 4 --cpus 4,5,6,7
run_owned "$LOGS/base-references" /usr/bin/python3 "$PRODUCER/references.py" \
    --build "$BASE" --out "$BASE_REFS" --wasmtime "$WASMTIME"
run_owned "$LOGS/base-manifests" /usr/bin/python3 "$PRODUCER/write_manifests.py" \
    --build "$BASE" --references "$BASE_REFS" --out "$BASE_CASES"
run_owned "$LOGS/full-Oz-build" /usr/bin/python3 "$PRODUCER/debug_oz_build.py" \
    --original "$BASE" --work "$OZ" --wasmtime "$WASMTIME" --jobs 4 --cpus 4,5,6,7
run_owned "$LOGS/full-Oz-references" /usr/bin/python3 "$PRODUCER/debug_oz_references.py" \
    --build "$OZ" --oracle "$BASE_REFS/report.json" \
    --original-cases "$BASE_CASES/cases.json" --out "$OZ_REFS" --wasmtime "$WASMTIME"
```

This executes real build/reference work if run; it is documentation here. The wrappers retain normal wait status and owned pidfd cleanup, while inner build commands keep the existing resource guards. Do not add `--resume-built` for a fresh Oz directory. Native/Oz binary identities can differ with absolute source/DWARF paths and host compiler; new receipts and artifact hashes must be retained rather than substituted into old reports. The source/bytecode and71-builtin/ABI checks remain required. Wasmtime42.0.1 used in the existing evidence has binary SHA `21f8e8f994a96d2267afe4a4c06a6302e78aca20e9438afbf01dd443fe32108b`.

To reproduce only Oz while the original qualified base still exists, the last two commands may instead use `--original $ARENA/python/work-r4`, its existing native/oracle and `manifests-r2/cases.json`, with fresh Oz/output/launcher paths. The historical `debug-full-Oz-r1` folder and receipts must remain untouched.

The original baseline's first failed parser helper (SHA `7e309f5c…`) was not retained as a source file; its original receipt remains available and this capture does not reconstruct it. The successful baseline helper `c15b192b…`, actual FullOz final/failed helpers and all direct dependencies are supplied. This limitation is separate from the fully matched FullOz scripts requested here.

The producer provides a static CPython embedding hook. The ROS SDK owns its ABI wrappers, extension, application and qualification. No ROS extension is linked into the qualified base fixtures. These reuse commands have not been executed for a ROS application.

**Actual retained Linux inputs**

All paths are inside the original SSH Linux container/cgroup. `/tmp/high-level-20261017` is a persistent arena alias. Do not rebuild on the workstation.

```sh
W=/tmp/high-level-20261017/python/work-r4
S=$W/sources/python/Python-3.14.8
N=$S/cross-build/x86_64-pc-linux-gnu
B=$S/cross-build/wasm32-wasip1
SDK=$W/sources/sdk/wasi-sdk-24.0-x86_64-linux
PRODUCER=/tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages/python
```

`$N/python` is the built CPython3.14.8 native oracle, not system Python. `$N/libpython3.14.a` and `$B/libpython3.14.a` are retained. Each build directory supplies its own generated `pyconfig.h`; never mix native and WASI headers. `$W/build-receipt.json` records exact successful compiler/linker argv, cwd and lifecycle. `$W/frozen-semantics/hll_frozen.h` is the tested minimal frozen table, with equivalent `timed` and `profile` directories.

**Register and link the SDK builtin**

An SDK-owned `roswasm_inittab.h` can contain:

```c
#ifndef ROSWASM_INITTAB_H
#define ROSWASM_INITTAB_H
#include <Python.h>
PyMODINIT_FUNC PyInit__roswasm_native(void);
#define HLL_REGISTER_EXTENSIONS() \
    PyImport_AppendInittab("_roswasm_native", PyInit__roswasm_native)
#endif
```

Compile `embedded_main.c` with the single argument `-DHLL_EXTENSION_HEADER="roswasm_inittab.h"`, add the SDK header directory, and link the extension object into the same final Wasm. The hook returns registration status and runs before `Py_PreInitialize`, argv decoding and `Py_InitializeFromConfig`. The SDK exports `PyInit__roswasm_native` and its CPython module definition. Python can then `import _roswasm_native`. Default producer builds register no extension.

The WASI extension compilation shape is:

```sh
"$SDK/bin/clang" -O2 -g -I"$B" -I"$S/Include" -I"$SDK_EXTENSION_INCLUDE" \
    -c "$SDK_EXTENSION_SOURCE" -o "$FRESH_SDK_WORK/roswasm-wasi.o"
```

For native references, use the native compiler from the receipt, `-I"$N" -I"$S/Include"`, and separate native objects. The SDK must supply a native backend/test backend: unresolved Wasm imports are not a native implementation. Record extra flags, sources and libraries. Use `linux_control.invoke` for actual compiler/linker execution, including process ownership and retirement.

Keep the exact successful CPython link recipe, relative bundled archives and cwd. This Linux-only recipe adds the hook/object without guessing CPython libraries. It must run in a released build window, with fresh SDK-owned paths; it is not a ROS qualification receipt.

```python
import json
from pathlib import Path
import sys
producer = Path('/tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages/python')
sys.path.insert(0, str(producer))
from linux_control import guard, invoke
work = Path('/tmp/high-level-20261017/python/work-r4')
receipt = json.loads((work / 'build-receipt.json').read_text())
sdk_work = Path('/absolute/fresh/SDK-owned-directory')
sdk_work.mkdir(parents=True, exist_ok=False)
guard(sdk_work)
sdk_include = Path('/absolute/SDK-owned-include-directory')

def relink(target, objects):
    suffix = '.wasm' if target == 'wasi' else '.native'
    old_output = str(work / 'artifacts' / ('python-semantics' + suffix))
    row = next(r for r in receipt['commands']
               if r['argv'][-1] == old_output and r['passed'] and r['retired'])
    argv = list(row['argv'])
    launcher = next(i for i, v in enumerate(argv) if v.endswith('/embedded_main.c'))
    argv[launcher:launcher] = ['-I' + str(sdk_include),
        '-DHLL_EXTENSION_HEADER="roswasm_inittab.h"', *map(str, objects)]
    argv[-1] = str(sdk_work / ('ros-python' + suffix))
    return invoke(argv, sdk_work / ('link-' + target), cwd=row['cwd'])

# Compile matching objects through invoke() first, then, for example:
# relink('wasi', [sdk_work / 'roswasm-wasi.o'])
# relink('native', [sdk_work / 'roswasm-native.o', native_backend_object])
```

This preserves qualified base outputs and receipts. A new application needs a new frozen header and output. `freeze_fixture.py` intentionally accepts only `semantics`, `timed`, `profile`: stage an SDK-private copy and chosen entry source, add the actual module closure, and run it with `$N/python`. Existing entry command: `$N/python $PRODUCER/freeze_fixture.py --cpython-source $S --entry semantics --out /absolute/fresh/frozen-dir`. Replace the frozen include directory when relinking. Code objects are version-specific; record source/bytecode/header hashes and keep assertions enabled during semantic qualification.

**Minimal frozen and stock-CLI file contracts**

The three tested embedded programs use an empty search path and no preopens. Their seven custom frozen modules are encodings, encodings.aliases, encodings.utf_8, encodings.ascii, encodings.latin_1, hll_common and hll_app, plus upstream frozen startup modules. This is not full stdlib and proves no dataclasses/typing/traceback/JSON/queue/asyncio support. Package flags, transitive/delayed imports and C builtins must be included and tested; see [ros-stdlib-source-notes.md](ros-stdlib-source-notes.md).

The unmodified stock command is `$W/artifacts/cpython-3.14.8-standard.wasm`. Actual full source stdlib is `$S/Lib`; generated WASI sysconfig lives at `$B/$(cat "$B/pybuilddir.txt")`. The build receipt inventories 1,870 `.py` files, not every package-data file. The minimal 46-case report does not qualify an installed full-stdlib bundle, all-module imports or stock CLI with that bundle.

| Guest path | Retained Linux input | Separate bundle contract |
| --- | --- | --- |
| `/python/lib/python3.14` | `$S/Lib`, including non-Python package data | Inventory all copied files and hashes |
| `/python/sysconfig` | `$B/$(cat "$B/pybuilddir.txt")` | Matching `_sysconfigdata_*.py` |

For stock CLI set `PYTHONHOME=/python`, `PYTHONPATH=/python/sysconfig`, and `-S -B -X utf8`. `-c` still needs encodings; `-E`/`-I` ignore these environment settings. The frozen launcher ignores Python configuration environment and keeps an empty search path, so mounts do not silently extend it.

Wasmtime42 maps `--dir HOST::GUEST`. UWVM uses `--wasip1-mount-dir GUEST HOST` and `--wasip1-add-or-replace-environment NAME VALUE` before `--run`. Mount only selected bundle directories, not host root. Preopens add filesystem capabilities; these spellings alone do not promise read-only enforcement. Configure rights separately and qualify startup/import/operation/denial cases before publishing that deployment. The separately executed stock-CLI qualification now passed 16 representative probes in each of native CPython, Wasmtime and the three fd_readdir-fixed UWVM products (80 total); see [the r2 results](stock-stdlib-r2-results.md) for exact bundle, flags, three ROS grants and immutable receipts. That result does not change the zero-preopen frozen manifests or qualify every stdlib module.

**ABI and ownership**

Use checked contiguous `Py_buffer` values for synchronous bytes/memoryview calls; reject negative/oversized lengths before conversion. Current ROS ABI has i32 memory index and i64 offset/length/handle, 8-byte little-endian output slots; wasm32 pointers require `(uint64_t)(uintptr_t)` zero extension. Actual SDK ABI JSON/C headers are authoritative. Keep bounds, principal/handle/ticket scope and error conversion. Do not retain pointers after `PyBuffer_Release`; asynchronous operations need owned copies or explicit retained owners. Explicit resource close and serialization belong to the SDK.

No `_ctypes`, libffi, dynamic extension, pip, native guest concurrency, socket, subprocess or complete native-rclpy support is implied. WASI single-thread stubs do not create native concurrency. C DWARF describes CPython, not Python locals. Any `sys.settrace` proof is separate Python language tracing; a debugger bridge also needs source/locals/line/restart evidence.

**Exact FullOz debug producer source and a fresh rebuild**

The real executed `debug_oz_build.py` is supplied byte-for-byte at the ordinary producer path and in [the preserved layout](debug-producer/evidence-sources/executed-layout/test/0050.high_level_languages/python/debug_oz_build.py), SHA `f3b10b16d5df0972b925b5d612c74f2fa6358f5633ad56b52ba6ebffd746bf03`. The preserved layout also contains direct/transitive helper imports, reference scripts and fixture sources; the first failed Oz/minimal helpers are stored separately by SHA. [REBUILD.md](debug-producer/REBUILD.md) contains the full Linux CLI chain for a fresh O2 native/base build, native/reference manifests and a fresh complete71-builtin Oz debug build.

For an existing qualified base, the fresh Oz build command is `/usr/bin/python3 "$PRODUCER/debug_oz_build.py" --original "$BASE" --work "$FRESH_OZ" --wasmtime "$WASMTIME" --jobs 4 --cpus 4,5,6,7`, run through the documented owned-process wrapper in the original64GiB cgroup. `$BASE` must have its real build receipt, qualified native3.14.8, original six artifacts, SDK24, source archive and frozen-semantics header. The matching reference command is `/usr/bin/python3 "$PRODUCER/debug_oz_references.py" --build "$FRESH_OZ" --oracle "$BASE_REFS/report.json" --original-cases "$BASE_CASES/cases.json" --out "$FRESH_OZ_REFS" --wasmtime "$WASMTIME"`. These new-directory commands are documented, not rerun for this delivery; the actual final old invocation used `--resume-built` after a metadata-parser failure, which is preserved in its receipts. Do not rerun against the historical output folder or substitute a fresh artifact SHA into the old evidence.

The reused native CPython SHA is `f36bef8c90257efac58a0ebc1e5c0f5231ab0f5c00246fdac9c014de123d837c`; the exact Python-bytecode header SHA is `2bfb80b80b4651b152926b9e5bbf7de80545a87ff5c00bd6e0559f23f191553e`. Both paths and the frozen source/marshal manifest, compiler/sysroot pins, complete module config and macro/Makefile captures are in the rebuild guide. FullOz is a distinct C optimization configuration, never a replacement claim that the O2 performance module passed DBG.

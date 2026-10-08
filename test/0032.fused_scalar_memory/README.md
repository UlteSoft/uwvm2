# Typed scalar Core3 memory gate

This directory contains new memory64/multi-memory syntax and independently calculated expected integer bit patterns. No file in this directory is a native PASS receipt. The product candidate removes the second raw memarg read for 23 scalar operations; other normalized families, the common typed core and pre-effect admission remain incomplete.

`fixtures/scalar_memory32_memory64_all23.wast` covers every scalar load/store in memory32 and memory64 (46 assertions), including packed signed extension and floating bit reinterpretation. Boundary WAST checks u64 max offset, overflow/OOB, invalid address/value width, alignment and memory32 offset. Five exact malformed binaries preserve reserved/truncated/overflow memargs. `unused_invalid_function_32.wat` places an unreferenced invalid function at index31 beyond lazy adjacent warmup's16-function budget. Its CLI wrapper appends `_start` index32 calling original entry0, and never changes the original function indices.

The provider-memory effect fixture requires a real same-instance component observer: provider byte0 starts0x41; rejected invalid module must not apply active data0x58. A CLI exit alone cannot observe that byte and is expressly not certified by this runner. Formal parsed-source-before-init admission is owned separately.

The C++ schema component uses fast_io output and 46 constexpr DATA controls, including memory64 i32.store consuming12 bytes. It is not a validator/IR/runtime test by itself.

Linux keeper uses the existing64GiB/swap0 cgroup and verified CPU set, and must supply actual current source/SDK/provider/product pins. The runner reuses the existing cgroup gate, configurations and original assertion wrapper; it does not implement a new native guardian. No local Mac native execution is authorized by this directory. Example after fresh matching product build:

```
python3 test/0032.fused_scalar_memory/run_actual_cli.py <new-evidence-dir> --uwvm <actual-product> --wat2wasm <actual-tool> --wast2json <actual-tool> --source-id <actual-cut-id> --configuration jit-full-instruction
```

Use `--ros` for ROS, `--wasmtime` for a separately pinned same-platform reference and `--combine-matrix` for ordinary interpreter combine/delay axes. Select finite cells first. The unused-invalid test may expose the known lazy admission blocker; preserve failures and do not silently omit it. Negative runtime traps first pass actual pure validation and then require a specific memory-trap diagnostic; unexpected signal/compilation errors do not count as semantic success. Known fast_io fatal SIGILL/SIGABRT may be accepted only with that exact memory diagnostic. Feature-off rows exercise both memory64 and multi-memory independently.

Generated ASM and performance qualification are separate gates. Inspect actual IR/object code for selected32/64 address widths, u64 overflow and signed extension; retain the original low-level memory safety/protection behavior and ensure no new guest guard/lock/callback. Compiler source hashes and positive process exits do not prove unchanged hot-path performance. All platforms/provider combinations not actually tested remain pending.

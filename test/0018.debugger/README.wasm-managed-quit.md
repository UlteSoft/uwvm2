The managed-quit regression stops a real full-JIT Wasm function at its first
opcode, then sends `quit`. Its default matrix uses 1 and 10000 i32 locals with
both `instruction` and `unwind` call-stack policies. The large frame targets the
RISC-V shutdown exception failure without issuing any ASM command.

Run under the verified Linux test cgroup:

```sh
python3 test/0018.debugger/run_wasm_managed_quit_cli.py \
  --source-root "$SOURCE" --binary "$VM" --wasm-tools "$WASM_TOOLS" \
  --out "$OUTPUT"
```

For ROS, add `--ros`; it selects `-Raot`. Ordinary uwvm2 explicitly selects
full LLVM JIT. For QEMU, add `--runner-prefix-json "$PREFIX"`, where the JSON
array specifies the qualified QEMU executable, target sysroot and target
library search path. Use repeated `--local-count` and `--call-stack-policy`
options to select a subset.

Every case requires the actual breakpoint at function 0, byte offset 0, followed
by exit code 0 within 30 seconds of `quit`. An uncaught exception, signal,
timeout or forced kill fails the command. `result.json` and per-case transcripts
retain observed exit codes. Only a successful outer keeper receipt qualifies a
completed test; a partial result file does not.

The shared opcode/operand/deep-stack console now uses the same strict quit
oracle. Previously it discarded nonzero exit codes and could accept a timeout
after killing the child. Earlier operand-value checks remain value evidence;
they do not independently establish clean shutdown.

This test covers managed cancellation of actual paused Wasm frames. It does
not qualify checkpoint restoration, every Core 3 feature, all architectures,
or public ASM access to the VM implementation.

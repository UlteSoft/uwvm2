# Windows LLVM-full debug server

On Windows x64, a normal `xmake` build and `xmake install -o <directory>` include both `uwvm.exe` and `uwvm-debug-server.exe`. To build them individually, run `xmake b uwvm` and `xmake b uwvm-debug-server`. The broker is a separate host program; it does not link the Wasm runtime or LLVM JIT.

From a trusted Windows console, start a Wasm program with an explicit LLVM-full run configuration:

```powershell
.\uwvm-debug-server.exe serve .\uwvm.exe -m run -Rct 0 -Rllvm-call-stack unwind -Rllvm-cache-path disable --run .\program.wasm
```

The broker prints a private pipe name and per-session capability to that console. In a second **host** console, connect with those exact values and type debugger commands (`status`, `pause`, `threads`, `step source`, `step wasm`, `step asm`, `replace_function`, `quit`):

```powershell
.\uwvm-debug-server.exe connect '<printed pipe name>' '<printed capability>'
```

`serve` must launch `uwvm.exe` itself. It passes a connected numeric HANDLE through a restricted inherited-handle list, assigns the child to a Job object before it runs, and never passes the pipe name or capability to the guest. The VM checks that the pipe belongs to its direct parent and is neither stdio nor a guest-visible WASI handle. The broker accepts only same-owner local clients outside the guest Job; possessing the capability alone does not authorize a guest descendant. Messages are bounded to 512 bytes and replies to 64 KiB. A peer disconnect releases the debugger; `quit` detaches it. The endpoint is available only in LLVM JIT full mode; other modes fail closed.

Direct `-m debug-jit` uses the protected host console and does not need this broker. On Linux and macOS, use the platform-specific host server rather than this Windows executable.

# X86 guaranteed tail calls and stack arguments

The native Core 3 tail ABI requires an X86 provider which correctly lowers
`musttail call tailcc` with differing parameter layouts and modified stack
arguments. `musttail` is retained; changing it into an ordinary call/return is
not a workaround.

The tested installed LLVM identifies itself as `23.0.0git`, revision
`4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89`. In
`X86ISelLoweringCall.cpp`, its guaranteed-TCO path assigns
`IsSibcall = IsMustTail`. This incorrectly skips both outgoing stack stores and
return-address relocation. The actual UWVM object for the 24-i64-parameter
regression contains register assignments followed by a jump, with no stores for
the remaining arguments. The equivalent ROS LLVM 23.1.1 object contains them.

[llvm-x86-tailcc-stack-arguments.patch](patches/llvm-x86-tailcc-stack-arguments.patch)
is a minimal backport against that exact source revision. It requires the
existing sibling-eligibility check before taking the shortcut. It has not yet
been qualified by rebuilding that installed provider; passing results below use
the separate stable provider. Do not mark an installed provider fixed based only
on applying the patch.

The [LLVM call rules](https://llvm.org/docs/LangRef.html#call-instruction) permit
differing prototypes for `musttail` under `tailcc`, while prohibiting an implicit
sret conversion. The bundled stable provider's implementation uses the actual
sibling-eligibility check. Related upstream history includes
[LLVM's X86 sibling-call correction](https://github.com/llvm/llvm-project/pull/176470).
That older backport is background context, not a claim that it alone fixes the
later tested revision.

`test/0014.llvm_jit/run_llvm23_x86_tailcc.py` compiles and runs the standalone
LLVM IR regression in both small and large code models. Each successful run
executes 1,000,001 transfers between functions with different prototypes,
checking every stack argument. Run it inside the SSH Linux test cgroup with
`--llc`, `--clang`, and a fresh output directory. `--expect-broken` records the
negative control; it does not qualify a provider.

Observed results:

- Installed 23.0.0git: both code models fail (negative control).
- Bundled 23.1.1-uwvm-ros.7: both code models pass.
- Actual ROS full-JIT Wasm tests also pass the stack/vector parameter cases.
- Ordinary-product builds may select a separate qualified LLVM installation via
  the test runner's `UWVM_TEST_LLVM_BUILD` and `UWVM_TEST_LLVM_SOURCE_INCLUDE`.
  This does not replace the user's installed toolchain or change product identity.

X86 providers older than 23.1 retain the established private typed ABI unless
built with `UWVM_LLVM_X86_TAILCC_FIXED`. Set that macro only after qualifying the
actual linked provider with the regression and the Wasm tail-call suites.
Unsupported tail prototypes fail emission on the older ABI rather than executing
known-corrupt generated code. The backport flag is included in native cache
identity. Runtime ABI v22 rejects v20/v21 objects from the previous typed ABI
policies. Raw host-entry ABI remains unchanged.

This is not a full cross-platform qualification. Aggregate-result tail calls
still need an explicit result-buffer ABI, and non-X86 targets require their own
live JIT/QEMU checks.

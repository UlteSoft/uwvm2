# Native guest EH host capability check

`run_native_exception_host_capabilities.py` syntax-checks the actual runtime
host header using Clang's real target and exception-model options. It does not
inject `__ARM_DWARF_EH__` or link ARM32 code to the existing EHABI provider.
The test captures compiler macros and checks both ARM and Thumb:

| Compiler model | Host helper eligibility |
| --- | --- |
| Default ARM EHABI | false |
| Explicit DWARF exceptions | true |
| DWARF with C++ exceptions disabled | false |
| SjLj exceptions | false |

AArch64, RISC-V64 and i386 also remain eligible under their default DWARF model.
This is only the host ABI prerequisite. The compiler independently checks the
actual LLVM TargetMachine for ELF/MachO and `DwarfCFI`, and the native C++ runtime
and unwinder must match the selected exception model. An ARM Linux target whose
TargetMachine still selects ARM EHABI is therefore rejected even when the
application was compiled with `-fdwarf-exceptions`.

The policy follows Clang's [exception-model macro generation](https://github.com/llvm/llvm-project/blob/main/clang/lib/Frontend/InitPreprocessor.cpp)
and LLVM libunwind's [ARM EHABI selection](https://github.com/llvm/llvm-project/blob/main/libunwind/include/__libunwind_config.h).
LLVM libc++abi has [different personality signatures for DWARF and ARM EHABI](https://github.com/llvm/llvm-project/blob/main/libcxxabi/src/cxa_personality.cpp),
so an ARM architecture macro alone cannot establish ABI compatibility.

The former header rejected ARM EABI even when the compiler selected DWARF.
The regression check fails on that frozen r216 header's ARM/DWARF assertion;
the corrected headers in both repositories pass all eleven syntax profiles.
This check does not claim ARM32 DWARF runtime execution or native JIT support
has been qualified with the available Linux EHABI sysroots.

The complete 86-file r217 evidence archive, including the old-header failure,
compiler macros, exact commands and both new-header results, is
`build/wasm3-evidence/native-exception-host-arm-capabilities-r217.tar.gz`
in the ordinary repository. SHA-256:
`f76fc8cd7e877b9fa82743a931dfdb3a3ec5198a86a875fb2938c9937bd9b10a`.
Every archived member was verified and rechecked against its remote original.

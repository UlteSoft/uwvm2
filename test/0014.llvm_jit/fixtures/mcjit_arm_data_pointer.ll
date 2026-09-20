; RuntimeDyld acceptance alone can miss endianness errors. Load this for both
; arm and armeb, map external_word to 0x11223344, and check the relocated word.
; LLVM 22/23's ELF ARM resolver writes ABS32 as little-endian even for armeb.
; The native capability guard must reject that ABI until its loader is repaired;
; a failing negative control is not a successful big-endian VM execution test.
@external_word = external global i32
@pointer_word = global ptr @external_word, align 4

; Checks are in mcjit_arm_data_pointer.check because llvm-rtdyld requires
; a leading '#' rather than LLVM IR's ';' comment syntax.

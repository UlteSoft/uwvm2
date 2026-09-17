; RUN: llc -mtriple=riscv64-linux-gnu -mattr=+m,+a,+f,+d -filetype=obj %s -o %T/local-symbols.o
; RUN: llvm-rtdyld -verify -triple=riscv64-linux-gnu -map-section=local-symbols.o,.text=0x20000 -map-section=local-symbols.o,.eh_frame=0x40000 -check=%S/rtdyld-local-symbols.check %T/local-symbols.o
; RUN: llc -mtriple=riscv32-linux-gnu -mattr=+m,+a,+f,+d -filetype=obj %s -o %T/local-symbols.o
; RUN: llvm-rtdyld -verify -triple=riscv32-linux-gnu -map-section=local-symbols.o,.text=0x20000 -map-section=local-symbols.o,.eh_frame=0x40000 -check=%S/rtdyld-local-symbols.check %T/local-symbols.o
; RUN: llc -mtriple=loongarch64-linux-gnu -filetype=obj %s -o %T/local-symbols.o
; RUN: llvm-rtdyld -verify -triple=loongarch64-linux-gnu -map-section=local-symbols.o,.text=0x20000 -map-section=local-symbols.o,.eh_frame=0x40000 -check=%S/rtdyld-local-symbols.check %T/local-symbols.o
;
; These ELF writers emit separate local ".L0 " symbol entries for the two
; FDE start addresses. ELF relocations refer to symbol indices, not names.
; Looking up either entry in RuntimeDyld's name map aliases it to the second
; function and loses the first function's unwind information. The first FDE
; must cover first, even though a same-named local labels second as well.
define i32 @first(i32 %value) #0 {
  %result = add i32 %value, 1
  ret i32 %result
}
define i32 @second(i32 %value) #0 {
  %result = add i32 %value, 2
  ret i32 %result
}
attributes #0 = { noinline nounwind uwtable(async) }

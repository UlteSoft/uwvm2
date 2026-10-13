# Actual llvm-mc inputs, not hand-built Mach-O records or executable owners.
# Assemble independently on the remote keeper:
#   llvm-mc -triple=x86_64-apple-darwin -filetype=obj -o x64.o this.s
#   llvm-mc -triple=arm64-apple-darwin -filetype=obj -o arm64.o this.s
# Keep each actual command/tool/object identity. Never run these bytes as code.

.section __TEXT,__prov_first,regular,pure_instructions
.p2align 3
.space 16
.globl first
first:
Lfirst:
.space 32

.section __TEXT,__prov_second,regular,pure_instructions
.p2align 3
.space 24
.globl second
second:
Lsecond:
.space 32

# The DEBUG attribute deliberately forces real local section-ordinal
# relocations even though the object also has exported atom symbols.
.section __DWARF,__debug_ranges,regular,debug
.quad Lfirst+3
.quad Lfirst+9
.quad Lsecond+5
.quad Lsecond+13
.quad 0
.quad 0

# This is a formal DWARF5 address-table header plus three addresses. Omitting
# DEBUG here is intentional: actual LLVM Mach-O writers must retain the
# external first/second atom relocations and their in-place signed addends.
.section __DWARF,__debug_addr,regular
.long 28
.short 5
.byte 8
.byte 0
.quad first+7
.quad second+11
.quad second-8

# A separate non-DWARF data component exercises the real 4-byte unsigned
# relocation with an in-place negative addend. This is not a DWARF loclist.
.section __DATA,__prov_w32,regular
.long second-4

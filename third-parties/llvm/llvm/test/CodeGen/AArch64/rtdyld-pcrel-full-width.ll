; RUN: llc -mtriple=aarch64-linux-gnu -filetype=obj %s -o %T/uwvm-aarch64-pcrel-le.o
; RUN: llvm-rtdyld -verify -triple=aarch64-linux-gnu -map-section=uwvm-aarch64-pcrel-le.o,.text=0x40000 -dummy-extern=external_pos=0x54320 -dummy-extern=external_neg=0x12340 -dummy-extern=external_byte_pos=0x54323 -dummy-extern=external_byte_neg=0x12343 -check=%S/rtdyld-pcrel-le.check %T/uwvm-aarch64-pcrel-le.o
; RUN: llc -mtriple=aarch64_be-linux-gnu -filetype=obj %s -o %T/uwvm-aarch64-pcrel-be.o
; RUN: llvm-rtdyld -verify -triple=aarch64_be-linux-gnu -map-section=uwvm-aarch64-pcrel-be.o,.text=0x40000 -dummy-extern=external_pos=0x54320 -dummy-extern=external_neg=0x12340 -dummy-extern=external_byte_pos=0x54323 -dummy-extern=external_byte_neg=0x12343 -check=%S/rtdyld-pcrel-be.check %T/uwvm-aarch64-pcrel-be.o
;
; AAELF64 requires LDR literal displacement bits 20:2 and ADR bits 20:0.
; Both signs exceed 4 KiB; ADR also exercises nonzero immlo. The original
; resolver masked these offsets with 0xffc, silently referencing wrong memory.
; https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst
module asm ".text"
module asm ".p2align 2"
module asm ".globl load_pos"
module asm "load_pos: ldr x0, external_pos"
module asm ".globl load_neg"
module asm "load_neg: ldr x1, external_neg"
module asm ".globl adr_pos"
module asm "adr_pos: adr x2, external_byte_pos"
module asm ".globl adr_neg"
module asm "adr_neg: adr x3, external_byte_neg"

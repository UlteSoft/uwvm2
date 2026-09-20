; AAELF64: LDR literal uses displacement bits [20:2]; ADR uses [20:0].
; Keep positive/negative offsets above 4 KiB and nonzero ADR low bits in the
; external mappings. Near-only tests conceal RuntimeDyld's old 0xffc mask.
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

.abiversion 1
.text
.p2align 2
.globl owned_ppc64_body
.type owned_ppc64_body,@notype
owned_ppc64_body:
  li 3,42
  blr
.Lowned_end:
.size owned_ppc64_body,.Lowned_end-owned_ppc64_body
.section .opd,"aw",@progbits
.p2align 3
.globl owned_ppc64_v1
.type owned_ppc64_v1,@function
owned_ppc64_v1:
  .quad owned_ppc64_body
  .quad .TOC.@tocbase
.size owned_ppc64_v1,8

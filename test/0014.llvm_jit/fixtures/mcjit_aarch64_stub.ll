; AArch64 instructions are always little-endian, including an ELF BE target.
; RuntimeDyld must not emit its far-call stub using the data-byte-order helper.
; Check the BR X16 instruction's actual bytes, not merely successful loading.
declare void @external_target()
define void @entry() "disable-tail-calls"="true" {
  call void @external_target()
  ret void
}

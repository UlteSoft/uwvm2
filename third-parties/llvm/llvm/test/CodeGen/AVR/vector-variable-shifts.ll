; RUN: llc -mtriple=avr -mcpu=atmega328p -O3 -verify-machineinstrs < %s | FileCheck %s
; A vector count cannot be truncated to scalar i8. Each lane needs its own
; count, and variable wide lanes must still receive AVR's loop expansion.
define void @byte_lanes(ptr %out, <2 x i8> %x, <2 x i8> %n) {
; CHECK-LABEL: byte_lanes:
; CHECK: ret
  %v = shl <2 x i8> %x, %n
  store <2 x i8> %v, ptr %out
  ret void
}
define void @word_lanes(ptr %out, <2 x i16> %x, <2 x i16> %n) {
; CHECK-LABEL: word_lanes:
; CHECK: ret
  %v = lshr <2 x i16> %x, %n
  store <2 x i16> %v, ptr %out
  ret void
}
define void @wide_lanes(ptr %out, <2 x i32> %x, <2 x i32> %n) {
; CHECK-LABEL: wide_lanes:
; CHECK-NOT: __ashrsi3
; CHECK: ret
  %v = ashr <2 x i32> %x, %n
  store <2 x i32> %v, ptr %out
  ret void
}

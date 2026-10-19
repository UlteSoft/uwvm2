;; Exception handling: exception_rethrow_ref. Source inventory: https://webassembly.github.io/spec/core/appendix/changes.html#release-3-0
;;
(module

 (import "wasi_snapshot_preview1" "clock_time_get" (func $clock (param i32 i64 i32) (result i32)))
 (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
 (memory $io (export "memory") 2)
 (data (i32.const 64) "Kernel ns: ")
 (data (i32.const 80) "Checksum: ")
 (data (i32.const 96) "\0a")
 (func $text (param $p i32) (param $n i32)
  (i32.store (i32.const 0) (local.get $p))
  (i32.store (i32.const 4) (local.get $n))
  (call $write (i32.const 1) (i32.const 0) (i32.const 1) (i32.const 8))
  if unreachable end)
 (func $number (param $v i64) (local $p i32)
  (local.set $p (i32.const 512))
  (loop $digits
   (local.set $p (i32.sub (local.get $p) (i32.const 1)))
   (i32.store8 (local.get $p) (i32.add (i32.wrap_i64 (i64.rem_u (local.get $v) (i64.const 10))) (i32.const 48)))
   (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
   (br_if $digits (i64.ne (local.get $v) (i64.const 0))))
  (call $text (local.get $p) (i32.sub (i32.const 512) (local.get $p))))
 (func $probe_cold (export "probe_cold") (result i32) i32.const 123)

 (global $iterations i32 (i32.const 10000))
 (global $kernel_repeats i32 (i32.const 1))
(tag $event (param i32)) (func $raise (param i32) (result i32) local.get 0 throw $event)
(func $kernel (export "kernel") (param $n i32) (result i64)
      (local $i i32) (local $sum i32) (local $value i32)

      (loop $again block $caught (result i32)
          try_table (catch $event $caught) block $reference (result (ref exn))
              try_table (catch_all_ref $reference)
               local.get $i i32.const 17 i32.add call $raise drop
              end unreachable
             end throw_ref end unreachable
         end local.set $value local.get $sum local.get $value i32.add local.set $sum
       local.get $i i32.const 1 i32.add local.tee $i
       local.get $n i32.lt_u br_if $again)
      local.get $sum i64.extend_i32_u)

 (func $start (export "_start") (local $answer i64) (local $elapsed i64) (local $remaining i32)
  i32.const 1 i64.const 1 i32.const 1024 call $clock if unreachable end
  global.get $kernel_repeats local.set $remaining
  (loop $repeat_kernel
   global.get $iterations call $kernel local.get $answer i64.add local.set $answer
   local.get $remaining i32.const 1 i32.sub local.tee $remaining br_if $repeat_kernel)
  i32.const 1 i64.const 1 i32.const 1032 call $clock if unreachable end
  i32.const 1032 i64.load i32.const 1024 i64.load i64.sub local.set $elapsed
  i32.const 64 i32.const 11 call $text local.get $elapsed call $number
  i32.const 96 i32.const 1 call $text
  i32.const 80 i32.const 10 call $text local.get $answer call $number
  i32.const 96 i32.const 1 call $text)
)

; LLVM X86 regression: musttail tailcc must move the return address and
; materialize outgoing stack arguments when prototypes differ. Wasm-facing
; coverage is run_wasm3_tail_transfer.py: stack-parameters and vector-parameters.
target triple = "x86_64-unknown-linux-gnu"
define tailcc i64 @small(i32 %n) noinline {
  %done = icmp eq i32 %n, 0
  br i1 %done, label %return, label %tail
return:
  ret i64 177
tail:
  %next = sub i32 %n, 1
  %result = musttail call tailcc i64 @large(i32 %next, i64 101, i64 102, i64 103, i64 104, i64 105, i64 106, i64 107, i64 108, i64 109, i64 110, i64 111, i64 112, i64 113, i64 114, i64 115, i64 116, i64 117, i64 118, i64 119, i64 120, i64 121, i64 122, i64 123, i64 124)
  ret i64 %result
}
define tailcc i64 @large(i32 %n, i64 %v0, i64 %v1, i64 %v2, i64 %v3, i64 %v4, i64 %v5, i64 %v6, i64 %v7, i64 %v8, i64 %v9, i64 %v10, i64 %v11, i64 %v12, i64 %v13, i64 %v14, i64 %v15, i64 %v16, i64 %v17, i64 %v18, i64 %v19, i64 %v20, i64 %v21, i64 %v22, i64 %v23) noinline {
  %ok0 = icmp eq i64 %v0, 101
  %ok1 = icmp eq i64 %v1, 102
  %all1 = and i1 %ok0, %ok1
  %ok2 = icmp eq i64 %v2, 103
  %all2 = and i1 %all1, %ok2
  %ok3 = icmp eq i64 %v3, 104
  %all3 = and i1 %all2, %ok3
  %ok4 = icmp eq i64 %v4, 105
  %all4 = and i1 %all3, %ok4
  %ok5 = icmp eq i64 %v5, 106
  %all5 = and i1 %all4, %ok5
  %ok6 = icmp eq i64 %v6, 107
  %all6 = and i1 %all5, %ok6
  %ok7 = icmp eq i64 %v7, 108
  %all7 = and i1 %all6, %ok7
  %ok8 = icmp eq i64 %v8, 109
  %all8 = and i1 %all7, %ok8
  %ok9 = icmp eq i64 %v9, 110
  %all9 = and i1 %all8, %ok9
  %ok10 = icmp eq i64 %v10, 111
  %all10 = and i1 %all9, %ok10
  %ok11 = icmp eq i64 %v11, 112
  %all11 = and i1 %all10, %ok11
  %ok12 = icmp eq i64 %v12, 113
  %all12 = and i1 %all11, %ok12
  %ok13 = icmp eq i64 %v13, 114
  %all13 = and i1 %all12, %ok13
  %ok14 = icmp eq i64 %v14, 115
  %all14 = and i1 %all13, %ok14
  %ok15 = icmp eq i64 %v15, 116
  %all15 = and i1 %all14, %ok15
  %ok16 = icmp eq i64 %v16, 117
  %all16 = and i1 %all15, %ok16
  %ok17 = icmp eq i64 %v17, 118
  %all17 = and i1 %all16, %ok17
  %ok18 = icmp eq i64 %v18, 119
  %all18 = and i1 %all17, %ok18
  %ok19 = icmp eq i64 %v19, 120
  %all19 = and i1 %all18, %ok19
  %ok20 = icmp eq i64 %v20, 121
  %all20 = and i1 %all19, %ok20
  %ok21 = icmp eq i64 %v21, 122
  %all21 = and i1 %all20, %ok21
  %ok22 = icmp eq i64 %v22, 123
  %all22 = and i1 %all21, %ok22
  %ok23 = icmp eq i64 %v23, 124
  %all23 = and i1 %all22, %ok23
  br i1 %all23, label %tail, label %bad
tail:
  %result = musttail call tailcc i64 @small(i32 %n)
  ret i64 %result
bad:
  ret i64 -1
}
define i32 @main() {
  %result = notail call tailcc i64 @small(i32 1000001)
  %bad = icmp ne i64 %result, 177
  %status = zext i1 %bad to i32
  ret i32 %status
}

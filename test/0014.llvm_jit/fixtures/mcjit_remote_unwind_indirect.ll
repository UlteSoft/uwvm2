; Paired diagnostic with host pointers supplied by the caller, as runtime bridge
; arguments rather than externally named PIC data. This distinguishes ordinary
; target C-ABI indirect calls from target-specific external-symbol relocations.
; Keep the direct-symbol fixture too: success here must not erase its failures.
declare void @llvm.trap()

define void @uwvm_remote_recursive(i32 %depth, ptr %state, ptr %callback) #0 {
entry:
  %done = icmp eq i32 %depth, 0
  br i1 %done, label %capture, label %recurse
capture:
  %mode = load volatile i32, ptr %state, align 4
  switch i32 %mode, label %normal [i32 1, label %trap
                                  i32 2, label %oob
                                  i32 3, label %convert]
normal:
  call void %callback()
  ret void
trap:
  call void @llvm.trap()
  unreachable
oob:
  %fault = load volatile i32, ptr inttoptr (i64 805306368 to ptr), align 4
  ret void
convert:
  %bits_ptr = getelementptr i8, ptr %state, i32 8
  %bits = load volatile i64, ptr %bits_ptr, align 8
  %value = bitcast i64 %bits to double
  %lower = fcmp ogt double %value, -2.147483649e+09
  %upper = fcmp olt double %value, 2.147483648e+09
  %valid = and i1 %lower, %upper
  br i1 %valid, label %converted, label %trap
converted:
  %integer = fptosi double %value to i32
  store volatile i32 %integer, ptr %state, align 4
  ret void
recurse:
  %next = sub i32 %depth, 1
  call void @uwvm_remote_recursive(i32 %next, ptr %state, ptr %callback)
  ret void
}

define void @uwvm_remote_entry(ptr %state, ptr %callback) #0 {
  call void @uwvm_remote_recursive(i32 8, ptr %state, ptr %callback)
  ret void
}

attributes #0 = { noinline nomerge nooutline nounwind uwtable(async) "disable-tail-calls"="true" }

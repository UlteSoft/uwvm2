; RuntimeDyld remote-load execution probe, not a complete foreign ROS runtime.
; Keep physical recursive frames, then invoke the guest's native unwinder through
; a relocated external call. There are deliberately no instruction-stack bridges.
declare void @uwvm_host_capture()
declare void @llvm.trap()
@uwvm_remote_state = external global [16 x i8]

define void @uwvm_remote_recursive(i32 %depth) #0 {
entry:
  %done = icmp eq i32 %depth, 0
  br i1 %done, label %capture, label %recurse
capture:
  %mode = load volatile i32, ptr @uwvm_remote_state, align 4
  switch i32 %mode, label %normal [i32 1, label %trap
                                  i32 2, label %oob
                                  i32 3, label %convert]
normal:
  call void @uwvm_host_capture()
  ret void
trap:
  call void @llvm.trap()
  unreachable
oob:
  ; The guest reserves this address as PROT_NONE, so this is a real guarded
  ; access fault, not an optimizer exploiting a statically null dereference.
  %fault = load volatile i32, ptr inttoptr (i64 805306368 to ptr), align 4
  ret void
convert:
  %bits_ptr = getelementptr i8, ptr @uwvm_remote_state, i32 8
  %bits = load volatile i64, ptr %bits_ptr, align 8
  %value = bitcast i64 %bits to double
  ; The Wasm trapping-conversion range/NaN guard precedes fptosi. This fixture
  ; probes its trap-time CFI, not the full production translator or FP oracle.
  %lower = fcmp ogt double %value, -2.147483649e+09
  %upper = fcmp olt double %value, 2.147483648e+09
  %valid = and i1 %lower, %upper
  br i1 %valid, label %converted, label %trap
converted:
  %integer = fptosi double %value to i32
  store volatile i32 %integer, ptr @uwvm_remote_state, align 4
  ret void
recurse:
  %next = sub i32 %depth, 1
  call void @uwvm_remote_recursive(i32 %next)
  ret void
}

define void @uwvm_remote_entry() #0 {
  call void @uwvm_remote_recursive(i32 8)
  ret void
}

attributes #0 = { noinline nomerge nooutline nounwind uwtable(async) "disable-tail-calls"="true" }

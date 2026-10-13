; An exploratory LLVM statepoint fixture, not product JIT code.
; The real gc_reference is a tagged carrier rather than an addrspace(1)
; pointer. If a collector relies on RewriteStatepointsForGC, its generated
; IR needs a precise managed-reference representation or explicit roots.
declare void @possible_collection()

define ptr addrspace(1) @typed_reference(ptr addrspace(1) %ref) gc "statepoint-example" {
entry:
  call void @possible_collection()
  ret ptr addrspace(1) %ref
}

define i64 @integer_carrier(i64 %ref) gc "statepoint-example" {
entry:
  call void @possible_collection()
  ret i64 %ref
}

define { i64, i64 } @tagged_carrier({ i64, i64 } %ref) gc "statepoint-example" {
entry:
  call void @possible_collection()
  ret { i64, i64 } %ref
}

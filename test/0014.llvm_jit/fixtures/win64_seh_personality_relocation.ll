; GNU Win64 COFF codegen contract for the JIT-local Itanium SEH personality.
; Compile only inside the prescribed Linux cgroup using Clang's GNU x64 target.
; The product builds equivalent IR in native_exception_symbols.h.
target triple = "x86_64-w64-windows-gnu"

$__gxx_personality_seh0 = comdat any
@uwvm_guest_exception_typeinfo_v1 = external constant i8
declare void @escape()
declare i32 @uwvm_guest_seh_host_personality_v1(ptr, ptr, ptr, ptr) nounwind

define linkonce_odr i32 @__gxx_personality_seh0(ptr %a, ptr %b, ptr %c, ptr %d) nounwind comdat {
entry:
  %call = musttail call i32 @uwvm_guest_seh_host_personality_v1(ptr %a, ptr %b, ptr %c, ptr %d) nounwind
  ret i32 %call
}

define void @probe() personality ptr @__gxx_personality_seh0 {
entry:
  invoke void @escape() to label %done unwind label %landing
landing:
  %lp = landingpad {ptr, i32} catch ptr @uwvm_guest_exception_typeinfo_v1
  ret void
done:
  ret void
}

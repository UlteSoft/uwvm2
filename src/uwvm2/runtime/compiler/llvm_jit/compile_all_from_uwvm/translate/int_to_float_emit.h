// Included inside the translator's details namespace, after its FP target helpers.
[[nodiscard]] inline ::llvm::Value* emit_llvm_int_to_float(::llvm::IRBuilder<>& builder,
                                                                    ::llvm::Value* value,
                                                                    ::llvm::Type* destination,
                                                                    bool is_signed) noexcept
{
    // Unlike ordinary uwvm2, ROS pins LLVM 23.1.1: its X86 lowerFPToIntToFP
    // tracks FromUnsigned and ToUnsigned separately. Early LLVM 23 snapshots
    // incorrectly compiled uitofp(fptosi(-1.5 to i32)) as -1 instead of
    // float(0xffffffff), including i64/reverse-sign variants on AVX512DQ.
    // The previous constrained-conversion workaround is therefore unnecessary
    // for this audited backend. Keep the direct conversion/combining path; the
    // pinned-version check prevents silently using that old development build.
    // Recheck both signedness directions and widths at O0/O3 on SSE2, AVX2,
    // AVX512 with/without VL, x86-64 no-SSE, and i686 SSE2/x87 on each upgrade.
    // This changes ONLY integer-to-float emission, not NaN-preserving float
    // loads/stores, trapping/saturating conversion checks, or the FP environment.
    // Regression entry points: test/0014.llvm_jit/check_mixed_signedness_wasm.py
    // exercises the real CLI/cache; the raw target-codegen matrix also bypasses
    // this emitter so a newly broken LLVM combine cannot be hidden by it.
    return is_signed ? builder.CreateSIToFP(value, destination) : builder.CreateUIToFP(value, destination);
}

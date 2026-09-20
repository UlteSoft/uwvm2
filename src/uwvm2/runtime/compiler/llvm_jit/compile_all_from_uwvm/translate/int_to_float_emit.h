// Included inside the translator's details namespace, after its FP target helpers.
[[nodiscard]] inline ::llvm::Value* emit_llvm_int_to_float(::llvm::IRBuilder<>& builder,
                                                                    ::llvm::Value* value,
                                                                    ::llvm::Type* destination,
                                                                    bool is_signed) noexcept
{
#if LLVM_VERSION_MAJOR >= 23
    if((value->getType()->isIntegerTy(32u) || value->getType()->isIntegerTy(64u)) && llvm_wasm_fp_target(builder).isX86() &&
       llvm_wasm_has_native_fp_feature(builder, "sse2"))
    {
        // LLVM 23's X86 scalar FP round-trip combine can turn
        // uitofp(fptosi(x to i32)) into cvttps2dq/cvtdq2ps (or its f64 pair),
        // losing the UNSIGNED interpretation of the i32 result. For x=-1.5,
        // Wasm requires float(0xffffffff), not -1. LLVM 22.1.8 is unaffected.
        // With AVX512DQ, the same helper also handles i64 and the opposite
        // unsigned-to-signed round trip. Protect both directions and widths;
        // testing only negative i32 on an AVX2 host would miss those variants.
        // A zext-to-i64 followed by sitofp is NOT a durable workaround: LLVM
        // InstCombine folds it back into uitofp before the faulty DAG combine.
        // Constrain this conversion even if fptosi is not its immediate producer:
        // later optimization may expose the pattern through locals/selects/PHIs.
        // Constrained conversions retain the target's native conversion path;
        // the i32 x86-64 repair needs no bridge, extra branch or spill. Leave x87/software
        // FP and other ISAs on their existing legalization paths. Do not remove
        // this version guard until the mixed-signedness regression passes on
        // the replacement LLVM backend, including optimized and cached code.
        llvm_wasm_arithmetic_scope scope{builder};
        builder.setIsFPConstrained(true);
        builder.setDefaultConstrainedRounding(::llvm::RoundingMode::NearestTiesToEven);
        builder.setDefaultConstrainedExcept(::llvm::fp::ebStrict);
        builder.setConstrainedFPFunctionAttr();
        return is_signed ? builder.CreateSIToFP(value, destination) : builder.CreateUIToFP(value, destination);
    }
#endif
    return is_signed ? builder.CreateSIToFP(value, destination) : builder.CreateUIToFP(value, destination);
}

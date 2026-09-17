// Included inside the LLVM lowering namespace. SIMD values remain SSA values, never host-call buffers.
// The canonical <16 x i8> value stores literal Wasm bytes in memory order. Convert lane byte order at the
// edge of each operation on big-endian targets; keep SSA/PHIs as vectors, never as scalarized i128 pairs.
namespace simd_ir
{
struct emitter
{
    ::llvm::IRBuilder<>& b;

    [[nodiscard]] auto integer(unsigned bits, unsigned lanes = 0u) const noexcept
    { return ::llvm::FixedVectorType::get(b.getIntNTy(bits), lanes == 0u ? 128u / bits : lanes); }

    [[nodiscard]] auto floating(unsigned bits, unsigned lanes = 0u) const noexcept
    { return ::llvm::FixedVectorType::get(bits == 32u ? b.getFloatTy() : b.getDoubleTy(), lanes == 0u ? 128u / bits : lanes); }

    [[nodiscard]] bool little_endian() const noexcept
    { return b.GetInsertBlock()->getModule()->getDataLayout().isLittleEndian(); }

    [[nodiscard]] ::llvm::Triple target() const noexcept
    {
        ::llvm::Triple result{b.GetInsertBlock()->getModule()->getTargetTriple()};
        return result.getArch() == ::llvm::Triple::UnknownArch ? ::llvm::Triple{::llvm::sys::getDefaultTargetTriple()} : result;
    }

    [[nodiscard]] ::llvm::Value* intrinsic(::llvm::Intrinsic::ID id, ::llvm::Value* a) const noexcept
    { return b.CreateIntrinsic(id, {a->getType()}, {a}); }

    [[nodiscard]] ::llvm::Value* intrinsic(::llvm::Intrinsic::ID id, ::llvm::Value* a, ::llvm::Value* c) const noexcept
    { return b.CreateIntrinsic(id, {a->getType()}, {a, c}); }

    [[nodiscard]] ::llvm::Value* endian(::llvm::Value* value) const noexcept
    {
        auto type{value->getType()};
        if(little_endian() || type->getScalarSizeInBits() == 8u) { return value; }
        if(type->isVectorTy() && type->getPrimitiveSizeInBits() == 128u && target().isMIPS() && target().isArch32Bit() && has_feature("msa"))
        {
            // O32's generic v2i64 bswap expansion mis-materializes 64-bit masks. Native lane shuffles also
            // avoid its long shift/mask sequence, and retain the exact same per-lane byte-order contract.
            auto bytes{b.CreateBitCast(value, integer(8u))};
            auto shuffled{b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.mips.shf.b"), {},
                {bytes, b.getInt32(type->getScalarSizeInBits() == 16u ? 0xb1u : 0x1bu)})};
            if(type->getScalarSizeInBits() == 64u)
            {
                shuffled = b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.mips.shf.w"), {},
                    {b.CreateBitCast(shuffled, integer(32u)), b.getInt32(0xb1u)});
            }
            return b.CreateBitCast(shuffled, type);
        }
        return intrinsic(::llvm::Intrinsic::bswap, value);
    }

    [[nodiscard]] ::llvm::Value* lanes(::llvm::Value* raw, unsigned bits, bool fp = false) const noexcept
    {
        if(fp && (target().isARM() || target().isThumb()))
        {
            // ARM32 NEON flushes FP32 subnormals even with FPSCR.FZ clear. Use IEEE scalar VFP for operations
            // consuming FP lanes, but do not mark integer-only SIMD functions strict or inhibit their inlining.
            b.setIsFPConstrained(true);
            b.setDefaultConstrainedRounding(::llvm::RoundingMode::NearestTiesToEven);
            b.setDefaultConstrainedExcept(::llvm::fp::ebStrict);
            b.setConstrainedFPFunctionAttr();
        }
        auto value{endian(b.CreateBitCast(raw, integer(bits)))};
        return fp ? b.CreateBitCast(value, floating(bits)) : value;
    }

    [[nodiscard]] ::llvm::Value* pack(::llvm::Value* value) const noexcept
    {
        auto type{::llvm::cast<::llvm::FixedVectorType>(value->getType())};
        auto bits{type->getScalarSizeInBits()};
        return b.CreateBitCast(endian(b.CreateBitCast(value, integer(bits, type->getNumElements()))), integer(8u));
    }

    [[nodiscard]] ::llvm::Constant* number(::llvm::Type* type, ::std::uint64_t value) const noexcept
    { return ::llvm::ConstantInt::get(type, value); }

    [[nodiscard]] ::llvm::Value* slice(::llvm::Value* value, unsigned first, unsigned count, unsigned stride = 1u) const noexcept
    {
        ::llvm::SmallVector<int, 16> mask{};
        for(unsigned i{}; i != count; ++i) { mask.push_back(static_cast<int>(first + i * stride)); }
        return b.CreateShuffleVector(value, ::llvm::PoisonValue::get(value->getType()), mask);
    }

    [[nodiscard]] ::llvm::Value* concat(::llvm::Value* a, ::llvm::Value* c) const noexcept
    {
        auto count{::llvm::cast<::llvm::FixedVectorType>(a->getType())->getNumElements()};
        ::llvm::SmallVector<int, 16> mask{};
        for(unsigned i{}; i != 2u * count; ++i) { mask.push_back(static_cast<int>(i)); }
        return b.CreateShuffleVector(a, c, mask);
    }

    [[nodiscard]] ::llvm::Value* extend(::llvm::Value* raw, unsigned bits, bool high, bool sign) const noexcept
    {
        if(bits == 32u && target().isMIPS() && target().isArch32Bit() && has_feature("msa"))
        {
            auto words{lanes(raw, 32u)};
            if(high) { words = b.CreateShuffleVector(words, words, {2, 3, 2, 3}); }
            return widen_mips32_words(words, sign);
        }
        auto count{64u / bits};
        auto v{slice(lanes(raw, bits), high ? count : 0u, count)};
        return b.CreateIntCast(v, integer(bits * 2u), sign);
    }

    [[nodiscard]] ::llvm::Value* widen_mips32_words(::llvm::Value* words, bool sign) const noexcept
    {
        // LLVM 22 O32 MSA loses the low half when legalizing <2 x i32> -> <2 x i64>. Keep full-width vectors
        // and interleave each source word with its sign/zero word, avoiding the unsupported narrow carrier.
        auto fill{sign ? b.CreateAShr(words, number(words->getType(), 31u)) : ::llvm::Constant::getNullValue(words->getType())};
        auto pairs{little_endian() ? b.CreateShuffleVector(words, fill, {0, 4, 1, 5})
                                  : b.CreateShuffleVector(words, fill, {4, 0, 5, 1})};
        return b.CreateBitCast(pairs, integer(64u));
    }

    [[nodiscard]] ::llvm::Value* narrow(::llvm::Value* raw, unsigned bits, bool sign) const noexcept
    {
        auto v{lanes(raw, bits * 2u)};
        // Both narrow variants interpret their INPUT lanes as signed integers.
        auto low{number(v->getType(), sign ? (0ull - (1ull << (bits - 1u))) : 0ull)};
        auto high{number(v->getType(), (1ull << (sign ? bits - 1u : bits)) - 1ull)};
        v = intrinsic(::llvm::Intrinsic::smax, v, low);
        v = intrinsic(::llvm::Intrinsic::smin, v, high);
        return b.CreateTrunc(v, integer(bits, 64u / bits));
    }

    [[nodiscard]] ::llvm::Value* pairwise(::llvm::Value* raw, unsigned bits, bool sign) const noexcept
    {
        auto v{lanes(raw, bits)};
        auto type{integer(bits * 2u)};
        auto a{b.CreateIntCast(slice(v, 0u, 64u / bits, 2u), type, sign)};
        auto c{b.CreateIntCast(slice(v, 1u, 64u / bits, 2u), type, sign)};
        return b.CreateAdd(a, c);
    }

    [[nodiscard]] ::llvm::Value* test(::llvm::Value* raw, unsigned bits, bool bitmask) const noexcept
    {
        auto v{lanes(raw, bits)};
        if(bitmask && target().isAArch64() && !has_feature("neon"))
        {
            // LLVM 22's scalar legalization of bitcast <16 x i1> to i16
            // discards the upper eight predicates when NEON is disabled.
            // Build the Wasm lane mask directly from integer sign bits; do
            // not pack boolean vectors on this sub-ISA. NEON-enabled targets
            // keep their original vector mask lowering and hot-path cost.
            ::llvm::Value* result{b.getInt32(0u)};
            for(unsigned lane{}; lane != 128u / bits; ++lane)
            {
                auto value{b.CreateExtractElement(v, b.getInt32(lane))};
                auto sign{b.CreateLShr(value, ::llvm::ConstantInt::get(value->getType(), bits - 1u))};
                auto word{b.CreateZExtOrTrunc(sign, b.getInt32Ty())};
                result = b.CreateOr(result, b.CreateShl(word, b.getInt32(lane)));
            }
            return result;
        }
        auto zero{::llvm::Constant::getNullValue(v->getType())};
        auto pred{bitmask ? b.CreateICmpSLT(v, zero) : b.CreateICmpNE(v, zero)};
        if(!bitmask) { return b.CreateZExt(intrinsic(::llvm::Intrinsic::vector_reduce_and, pred), b.getInt32Ty()); }
        // bitcast <N x i1> to iN reverses the lane significance on big-endian targets.
        if(!little_endian())
        {
            ::llvm::SmallVector<int, 16> mask{};
            for(unsigned i{128u / bits}; i != 0u; --i) { mask.push_back(static_cast<int>(i - 1u)); }
            pred = b.CreateShuffleVector(pred, ::llvm::PoisonValue::get(pred->getType()), mask);
        }
        return b.CreateZExt(b.CreateBitCast(pred, b.getIntNTy(128u / bits)), b.getInt32Ty());
    }

    [[nodiscard]] ::llvm::Value* shift(::llvm::Value* raw, ::llvm::Value* count, unsigned bits, unsigned kind) const noexcept
    {
        auto v{lanes(raw, bits)};
        // LLVM overshifts are poison; Wasm masks the scalar count by lane width.
        auto n{b.CreateIntCast(b.CreateAnd(count, b.getInt32(bits - 1u)), b.getIntNTy(bits), false)};
        n = b.CreateVectorSplat(128u / bits, n);
        return kind == 0u ? b.CreateShl(v, n) : kind == 1u ? b.CreateAShr(v, n) : b.CreateLShr(v, n);
    }

    [[nodiscard]] ::llvm::Value* float_sign(::llvm::Value* raw, unsigned bits, bool negate) const noexcept
    {
        auto value{lanes(raw, bits)};
        if(target().isMIPS() && has_feature("msa"))
        {
            // Use one native bit instruction. FP abs/neg may treat NaNs specially, and LLVM 22 BE MSA can
            // mis-materialize a byte-vector sign mask after commuting bswap through a generic AND/XOR.
            auto name{negate ? (bits == 32u ? "llvm.mips.bnegi.w" : "llvm.mips.bnegi.d")
                             : (bits == 32u ? "llvm.mips.bclri.w" : "llvm.mips.bclri.d")};
            return pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID(name), {}, {value, b.getInt32(bits - 1u)}));
        }
        return pack(negate ? b.CreateXor(value, number(value->getType(), 1ull << (bits - 1u)))
                           : b.CreateAnd(value, number(value->getType(), (1ull << (bits - 1u)) - 1u)));
    }

    // Exact IEEE rounding using integer vectors for targets whose LLVM vector FP-rounding legalization is broken.
    // All shift counts are masked before evaluation: the unselected large/NaN lanes must not introduce poison.
    [[nodiscard]] ::llvm::Value* round_bits(::llvm::Value* raw_bytes, unsigned bits, ::llvm::Intrinsic::ID operation) const noexcept
    {
        if(bits == 64u && target().isArch32Bit() && has_feature("msa"))
        {
            // O32 also mis-materializes the i64 masks in generic bit rounding. MSA truncation is independent of
            // MSACSR's rounding mode; integers below 2^52 convert back exactly. Larger values are already integral.
            auto x{lanes(raw_bytes, bits, true)};
            auto call{[&](char const* name, ::llvm::ArrayRef<::llvm::Value*> args)
                { return b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID(name), {}, args); }};
            auto abs_fp{[&](::llvm::Value* v)
                { return b.CreateBitCast(call("llvm.mips.bclri.d", {b.CreateBitCast(v, integer(64u)), b.getInt32(63u)}), floating(64u)); }};
            auto fp{[&](double v) { return ::llvm::ConstantFP::get(floating(64u), v); }};
            auto q{call("llvm.mips.ftrunc.s.d", {x})};
            ::llvm::Value* rounded{call("llvm.mips.ffint.s.d", {q})};
            if(operation == ::llvm::Intrinsic::ceil)
            { rounded = b.CreateFAdd(rounded, b.CreateSelect(b.CreateFCmpOLT(rounded, x), fp(1.0), fp(0.0))); }
            else if(operation == ::llvm::Intrinsic::floor)
            { rounded = b.CreateFSub(rounded, b.CreateSelect(b.CreateFCmpOGT(rounded, x), fp(1.0), fp(0.0))); }
            else if(operation == ::llvm::Intrinsic::roundeven)
            {
                auto fraction{abs_fp(b.CreateFSub(x, rounded))};
                auto odd{b.CreateICmpNE(call("llvm.mips.slli.d", {q, b.getInt32(63u)}), ::llvm::Constant::getNullValue(integer(64u)))};
                auto up{b.CreateOr(b.CreateFCmpOGT(fraction, fp(0.5)), b.CreateAnd(b.CreateFCmpOEQ(fraction, fp(0.5)), odd))};
                auto unit{b.CreateSelect(b.CreateFCmpOLT(x, fp(0.0)), fp(-1.0), fp(1.0))};
                rounded = b.CreateFAdd(rounded, b.CreateSelect(up, unit, fp(0.0)));
            }
            auto result{b.CreateBitCast(b.CreateSelect(b.CreateFCmpOLT(abs_fp(x), fp(4503599627370496.0)), rounded, x), integer(64u))};
            auto sign_mask{call("llvm.mips.bseti.d", {::llvm::Constant::getNullValue(integer(64u)), b.getInt32(63u)})};
            result = b.CreateOr(b.CreateAnd(result, b.CreateNot(sign_mask)), b.CreateAnd(b.CreateBitCast(x, integer(64u)), sign_mask));
            auto quiet{call("llvm.mips.bseti.d", {result, b.getInt32(51u)})};
            return pack(b.CreateSelect(b.CreateFCmpUNO(x, x), quiet, result));
        }
        auto raw{lanes(raw_bytes, bits)};
        auto type{raw->getType()};
        auto n{[&](::std::uint64_t x) { return number(type, x); }};
        unsigned mantissa{bits == 32u ? 23u : 52u}, bias{bits == 32u ? 127u : 1023u};
        auto sign{b.CreateAnd(raw, n(1ull << (bits - 1u)))};
        auto magnitude{b.CreateAnd(raw, n((1ull << (bits - 1u)) - 1u))};
        auto exponent{b.CreateLShr(magnitude, n(mantissa))};
        auto negative{b.CreateICmpNE(sign, n(0u))};
        auto shift{b.CreateAnd(b.CreateSub(n(bias + mantissa), exponent), n(bits - 1u))};
        auto unit{b.CreateShl(n(1u), shift)};
        auto mask{b.CreateSub(unit, n(1u))};
        auto fraction{b.CreateAnd(raw, mask)};
        auto truncated{b.CreateAnd(raw, b.CreateNot(mask))};
        auto up{b.CreateICmpNE(fraction, n(0u))};
        auto small_up{b.CreateICmpNE(magnitude, n(0u))};
        if(operation == ::llvm::Intrinsic::ceil)
        { up = b.CreateAnd(up, b.CreateNot(negative)); small_up = b.CreateAnd(small_up, b.CreateNot(negative)); }
        else if(operation == ::llvm::Intrinsic::floor)
        { up = b.CreateAnd(up, negative); small_up = b.CreateAnd(small_up, negative); }
        else if(operation == ::llvm::Intrinsic::roundeven)
        {
            auto half{b.CreateLShr(unit, n(1u))};
            up = b.CreateOr(b.CreateICmpUGT(fraction, half), b.CreateAnd(b.CreateICmpEQ(fraction, half), b.CreateICmpNE(b.CreateAnd(truncated, unit), n(0u))));
            small_up = b.CreateICmpUGT(magnitude, n(static_cast<::std::uint64_t>(bias - 1u) << mantissa));
        }
        else { up = b.CreateICmpNE(raw, raw); small_up = up; }
        auto rounded{b.CreateAnd(b.CreateAdd(raw, b.CreateSelect(up, unit, n(0u))), b.CreateNot(mask))};
        auto small{b.CreateOr(sign, b.CreateSelect(small_up, n(static_cast<::std::uint64_t>(bias) << mantissa), n(0u)))};
        auto result{b.CreateSelect(b.CreateICmpULT(exponent, n(bias)), small,
            b.CreateSelect(b.CreateICmpULT(exponent, n(bias + mantissa)), rounded, raw))};
        auto nan{b.CreateICmpUGT(magnitude, n(bits == 32u ? 0x7f800000ull : 0x7ff0000000000000ull))};
        return pack(b.CreateOr(result, b.CreateSelect(nan, n(1ull << (mantissa - 1u)), n(0u))));
    }

    [[nodiscard]] ::llvm::Value* convert_sat(::llvm::Value* raw, unsigned fp_bits, bool sign) const noexcept
    {
        auto v{lanes(raw, fp_bits, true)};
        auto type{integer(32u, 128u / fp_bits)};
        // Saturating LLVM intrinsics handle NaN and infinities without creating FPToI poison.
        ::llvm::Value* result{b.CreateIntrinsic(sign ? ::llvm::Intrinsic::fptosi_sat : ::llvm::Intrinsic::fptoui_sat,
                                      {type, v->getType()}, {v})};
        if(fp_bits == 64u && !sign && (target().isPPC() || target().isLoongArch() || target().isMIPS()))
        {
            // LLVM 22 PPC/LoongArch (and related MIPS conversions) can return UINT_MAX for NaN in unsigned double saturation.
            // Classify bits explicitly and mask that hardware result back to Wasm/LLVM's required zero.
            auto bits{b.CreateBitCast(v, integer(64u))};
            auto magnitude{b.CreateAnd(bits, number(bits->getType(), 0x7fffffffffffffffull))};
            auto numeric{target().isMIPS() && target().isArch32Bit() ? b.CreateFCmpORD(v, v)
                : b.CreateICmpULE(magnitude, number(bits->getType(), 0x7ff0000000000000ull))};
            result = b.CreateAnd(result, b.CreateSExt(numeric, type));
        }
        return fp_bits == 64u ? concat(result, ::llvm::Constant::getNullValue(type)) : result;
    }

    [[nodiscard]] bool has_feature(::llvm::StringRef name) const noexcept
    {
        // Explicit attributes are authoritative. During initial native IR emission the full/lazy materializers have
        // not attached them yet: mirror their runtime/FP-ABI feature exclusions, not just physical CPU capabilities.
        auto function{b.GetInsertBlock()->getParent()};
        if(function->hasFnAttribute("target-features"))
        {
            auto features{function->getFnAttribute("target-features").getValueAsString()};
            bool enabled{};
            while(!features.empty())
            {
                auto pair{features.split(',')};
                if(pair.first.size() == name.size() + 1uz && pair.first.drop_front() == name) { enabled = pair.first.front() == '+'; }
                features = pair.second;
            }
            return enabled;
        }
#if (defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)) && !defined(__mips_msa)
        if(name == "msa") { return false; }
#endif
#if defined(__riscv) && !defined(__riscv_flen)
        if(name == "f" || name == "d" || name == "v") { return false; }
#endif
#if (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)) && !defined(__ALTIVEC__) && !defined(__linux__)
        if(name == "altivec" || name == "vsx") { return false; }
#endif
        static auto const features{::llvm::sys::getHostCPUFeatures()};
        auto found{features.find(name)};
        return found != features.end() && found->second;
    }

    [[nodiscard]] ::llvm::Value* swizzle(::llvm::Value* table, ::llvm::Value* indices) const noexcept
    {
        auto const target{this->target()};
        auto call{[&](::llvm::StringRef name, ::llvm::ArrayRef<::llvm::Type*> types, ::llvm::ArrayRef<::llvm::Value*> args)
        { return b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID(name), types, args); }};
        if(target.isX86() && has_feature("ssse3"))
        {
            // PSHUFB wraps bits 4..6, whereas Wasm zeros every index >= 16. Saturating +0x70 sets bit 7 for ALL
            // invalid indices and retains the low nibble for valid indices: one packed add plus one shuffle.
            auto controls{intrinsic(::llvm::Intrinsic::uadd_sat, indices, number(indices->getType(), 0x70u))};
            return call("llvm.x86.ssse3.pshuf.b.128", {}, {table, controls});
        }
        if(target.isAArch64() && has_feature("neon"))
        {
            // The LLVM intrinsic already follows LLVM lane order on BOTH endians (unlike the C NEON builtin wrapper).
            return call("llvm.aarch64.neon.tbl1", {integer(8u)}, {table, indices});
        }
        if((target.isARM() || target.isThumb()) && has_feature("neon"))
        {
            auto lo{slice(table, 0u, 8u)}, hi{slice(table, 8u, 8u)};
            auto half{[&](unsigned first) -> ::llvm::Value*
            {
                auto index{slice(indices, first, 8u)};
                return call("llvm.arm.neon.vtbl2", {}, {lo, hi, index});
            }};
            return concat(half(0u), half(8u));
        }
        ::llvm::Value* permuted{};
        if(target.isPPC() && has_feature("altivec"))
        {
            auto v{b.CreateBitCast(table, integer(32u))};
            permuted = b.CreateBitCast(call("llvm.ppc.altivec.vperm", {}, {v, v, little_endian() ? b.CreateNot(indices) : indices}), integer(8u));
        }
        else if(target.getArch() == ::llvm::Triple::systemz && has_feature("vector"))
        { permuted = call("llvm.s390.vperm", {}, {table, table, indices}); }
        else if(target.isLoongArch() && has_feature("lsx"))
        { permuted = call("llvm.loongarch.lsx.vshuf.b", {}, {table, table, indices}); }
        else if(target.isMIPS() && has_feature("msa"))
        { permuted = call("llvm.mips.vshf.b", {}, {indices, table, table}); }
        else if(target.isRISCV() && has_feature("v"))
        {
            auto type{::llvm::ScalableVectorType::get(b.getInt8Ty(), 16u)};
            auto insert{[&](::llvm::Value* value)
            { return b.CreateIntrinsic(::llvm::Intrinsic::vector_insert, {type, integer(8u)},
                                       {::llvm::PoisonValue::get(type), value, b.getInt64(0u)}); }};
            auto vl{b.getIntNTy(target.isArch64Bit() ? 64u : 32u)};
            auto value{call("llvm.riscv.vrgather.vv", {type, vl},
                            {::llvm::PoisonValue::get(type), insert(table), insert(indices), number(vl, 16u)})};
            permuted = b.CreateIntrinsic(::llvm::Intrinsic::vector_extract, {integer(8u), type}, {value, b.getInt64(0u)});
        }
        if(permuted != nullptr)
        { return b.CreateSelect(b.CreateICmpULT(indices, number(indices->getType(), 16u)), permuted, ::llvm::Constant::getNullValue(integer(8u))); }

        // Unsupported vector ISAs still lower correct SSA operations, without a host ABI or a helper call. Mask before
        // extractelement even on the unselected arm, so an invalid swizzle index can never introduce LLVM poison.
        ::llvm::Value* out{::llvm::PoisonValue::get(integer(8u))};
        for(unsigned i{}; i != 16u; ++i)
        {
            auto index{b.CreateExtractElement(indices, b.getInt32(i))};
            auto value{b.CreateExtractElement(table, b.CreateZExt(b.CreateAnd(index, b.getInt8(15u)), b.getInt32Ty()))};
            value = b.CreateSelect(b.CreateICmpULT(index, b.getInt8(16u)), value, b.getInt8(0u));
            out = b.CreateInsertElement(out, value, b.getInt32(i));
        }
        return out;
    }

    [[nodiscard]] ::llvm::Value* ordered_minmax(::llvm::Value* x, ::llvm::Value* y, unsigned bits, bool maximum) const noexcept
    {
        // LLVM 22 scalar minimum/maximum expansion can create an illegal i64
        // SETCC after legalization (ARM/SPARC V8/non-MSA MIPS and i686 x87).
        // PPC32 without VSX can even emit a 64-bit GPR load on a 32-bit CPU.
        // i386 without SSE2 also needs this for f32: LLVM otherwise emits
        // fminimumf/fmaximumf returning in native ST0, while generated Wasm
        // uses the private no-x87 integer-result ABI. That mismatch silently
        // reads unrelated EAX bits. Ordinary comparisons are integer-lowered
        // by the mandatory i386 pass, so this path needs no FP-result libcall.
        // Express its IEEE rules in ordinary IR instead: ordered comparison, signed-zero tie, and arithmetic NaN.
        auto xi{b.CreateBitCast(x, integer(bits))}, yi{b.CreateBitCast(y, integer(bits))};
        auto ordered{b.CreateSelect(maximum ? b.CreateFCmpOGT(x, y) : b.CreateFCmpOLT(x, y), xi, yi)};
        auto tied{maximum ? b.CreateAnd(xi, yi) : b.CreateOr(xi, yi)};
        auto normal{b.CreateSelect(b.CreateFCmpOEQ(x, y), tied, ordered)};
        auto value{b.CreateSelect(b.CreateFCmpUNO(x, y), number(integer(bits), bits == 32u ? 0x7fc00000ull : 0x7ff8000000000000ull), normal)};
        return pack(b.CreateBitCast(value, floating(bits)));
    }
};

// A single non-template switch avoids duplicating the emitter for every validator visitor instantiation.
[[nodiscard]] inline ::llvm::Value* emit_value(::llvm::IRBuilder<>& b, llvm_jit_simd_code op,
                                               ::llvm::Value* a = nullptr, ::llvm::Value* c = nullptr,
                                               ::llvm::Value* d = nullptr, unsigned lane = 0u,
                                               ::std::byte const* bytes = nullptr) noexcept
{
    using code = llvm_jit_simd_code;
    emitter e{b};
    ::llvm::IRBuilderBase::FastMathFlagGuard fp_guard{b};
    auto const binary{[&](unsigned bits, unsigned operation) noexcept -> ::llvm::Value*
    {
        auto x{e.lanes(a, bits)};
        auto y{e.lanes(c, bits)};
        return e.pack(operation == 0u ? b.CreateAdd(x, y) : operation == 1u ? b.CreateSub(x, y) : b.CreateMul(x, y));
    }};
    auto const float_binary{[&](unsigned bits, unsigned operation) noexcept -> ::llvm::Value*
    {
        auto x{e.lanes(a, bits, true)}, y{e.lanes(c, bits, true)};
        if(e.target().isMIPS() && e.has_feature("msa"))
        {
            // LLVM 22 BE MSA constrained arithmetic loops in instruction selection. Native
            // arithmetic intrinsics preserve NaN quieting and remain a single vector instruction.
            char const* const names[2][4]{{"llvm.mips.fadd.w", "llvm.mips.fsub.w", "llvm.mips.fmul.w", "llvm.mips.fdiv.w"},
                                         {"llvm.mips.fadd.d", "llvm.mips.fsub.d", "llvm.mips.fmul.d", "llvm.mips.fdiv.d"}};
            return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID(names[bits == 32u ? 0u : 1u][operation]), {}, {x, y}));
        }
        if(e.target().isLoongArch() && e.has_feature("lsx"))
        {
            // Generic constrained FP currently scalarizes to soft-float library calls on LoongArch.
            char const* const names[2][4]{{"llvm.loongarch.lsx.vfadd.s", "llvm.loongarch.lsx.vfsub.s", "llvm.loongarch.lsx.vfmul.s", "llvm.loongarch.lsx.vfdiv.s"},
                                         {"llvm.loongarch.lsx.vfadd.d", "llvm.loongarch.lsx.vfsub.d", "llvm.loongarch.lsx.vfmul.d", "llvm.loongarch.lsx.vfdiv.d"}};
            return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID(names[bits == 32u ? 0u : 1u][operation]), {}, {x, y}));
        }
        if(e.target().isLoongArch())
        {
            ::llvm::Value* result{::llvm::PoisonValue::get(x->getType())};
            for(unsigned i{}; i != 128u / bits; ++i)
            {
                auto value{emit_llvm_float_binary(b, b.CreateExtractElement(x, b.getInt32(i)), b.CreateExtractElement(y, b.getInt32(i)), operation)};
                result = b.CreateInsertElement(result, value, b.getInt32(i));
            }
            return e.pack(result);
        }
        llvm_wasm_arithmetic_scope arithmetic_scope{b};
        return e.pack(operation == 0u ? b.CreateFAdd(x, y) : operation == 1u ? b.CreateFSub(x, y) :
                      operation == 2u ? b.CreateFMul(x, y) : b.CreateFDiv(x, y));
    }};
    auto const cmp{[&](unsigned bits, ::llvm::CmpInst::Predicate pred, bool fp = false) noexcept -> ::llvm::Value*
    {
        auto x{e.lanes(a, bits, fp)};
        auto y{e.lanes(c, bits, fp)};
        return e.pack(b.CreateSExt(fp ? b.CreateFCmp(pred, x, y) : b.CreateICmp(pred, x, y), e.integer(bits)));
    }};
    auto const bin_intrinsic{[&](unsigned bits, ::llvm::Intrinsic::ID id, bool fp = false) noexcept -> ::llvm::Value*
    {
        auto x{e.lanes(a, bits, fp)}, y{e.lanes(c, bits, fp)};
        if((e.target().isARM() || e.target().isThumb() || e.target().getArch() == ::llvm::Triple::sparc ||
            (e.target().isMIPS() && !e.has_feature("msa")) ||
            // x86_64 without SSE has the same floating-libcall problem as
            // i386, plus its native C FP return ABI requires XMM0. Use integer
            // min/max selection; the target FP pass lowers its comparisons.
            (e.target().isX86() && !e.has_feature("sse2")) ||
            (e.target().isArch32Bit() && bits == 64u && e.target().isPPC() && !e.has_feature("vsx"))) &&
           (id == ::llvm::Intrinsic::minimum || id == ::llvm::Intrinsic::maximum))
        { return e.ordered_minmax(x, y, bits, id == ::llvm::Intrinsic::maximum); }
        auto value{e.intrinsic(id, x, y)};
        if(id == ::llvm::Intrinsic::minimum || id == ::llvm::Intrinsic::maximum)
        {
            // LLVM's non-strict minimum/maximum may forward an input sNaN unchanged (notably x86).
            // Wasm arithmetic NaNs must be quiet. Integer operations keep the quiet bit observable to optimization.
            auto raw{b.CreateBitCast(value, e.integer(bits))};
            auto nan{b.CreateFCmpUNO(x, y)};
            if(bits == 64u && e.target().isMIPS() && e.target().isArch32Bit() && e.has_feature("msa"))
            {
                // O32 BE's i64 splat-mask materialization can set a payload bit instead of the quiet bit.
                // Preserve canonical NaNs as canonical, not merely as some non-signaling NaN.
                auto quiet{b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.mips.bseti.d"), {}, {raw, b.getInt32(51u)})};
                return e.pack(b.CreateSelect(nan, quiet, raw));
            }
            auto quiet{b.CreateAnd(b.CreateSExt(nan, e.integer(bits)), e.number(e.integer(bits),
                bits == 32u ? 0x00400000ull : 0x0008000000000000ull))};
            return e.pack(b.CreateOr(raw, quiet));
        }
        return e.pack(value);
    }};
    auto const unary_intrinsic{[&](unsigned bits, ::llvm::Intrinsic::ID id, bool fp = false) noexcept -> ::llvm::Value*
    {
        auto value{e.lanes(a, bits, fp)};
        if(e.target().isMIPS() && !e.little_endian() && fp && (id == ::llvm::Intrinsic::ceil || id == ::llvm::Intrinsic::floor ||
            id == ::llvm::Intrinsic::trunc || id == ::llvm::Intrinsic::roundeven))
        {
            // Both strict and non-strict BE MSA rounding loop in LLVM 22. Other FP arithmetic remains native.
            return e.round_bits(a, bits, id);
        }
        if(fp && b.getIsFPConstrained())
        {
            auto constrained{::llvm::Intrinsic::not_intrinsic};
            switch(id)
            {
                case ::llvm::Intrinsic::sqrt: constrained = ::llvm::Intrinsic::experimental_constrained_sqrt; break;
                case ::llvm::Intrinsic::ceil: constrained = ::llvm::Intrinsic::experimental_constrained_ceil; break;
                case ::llvm::Intrinsic::floor: constrained = ::llvm::Intrinsic::experimental_constrained_floor; break;
                case ::llvm::Intrinsic::trunc: constrained = ::llvm::Intrinsic::experimental_constrained_trunc; break;
                case ::llvm::Intrinsic::roundeven: constrained = ::llvm::Intrinsic::experimental_constrained_roundeven; break;
                default: break; // abs is a bit operation and does not need constrained arithmetic.
            }
            if(constrained != ::llvm::Intrinsic::not_intrinsic)
            {
                auto function{::llvm::Intrinsic::getOrInsertDeclaration(b.GetInsertBlock()->getModule(), constrained, {value->getType()})};
                return e.pack(b.CreateConstrainedFPCall(function, {value}));
            }
        }
        auto result{e.intrinsic(id, value)};
        if(e.target().isRISCV() && fp && (id == ::llvm::Intrinsic::ceil || id == ::llvm::Intrinsic::floor ||
            id == ::llvm::Intrinsic::trunc || id == ::llvm::Intrinsic::roundeven))
        {
            // Without native vector rounding instructions LLVM keeps the large/NaN input lanes unchanged.
            // Quiet the latter explicitly, as required by Wasm (without changing abs/neg bit semantics).
            auto type{e.integer(bits)};
            auto quiet{b.CreateAnd(b.CreateSExt(b.CreateFCmpUNO(value, value), type),
                e.number(type, bits == 32u ? 0x00400000ull : 0x0008000000000000ull))};
            return e.pack(b.CreateOr(b.CreateBitCast(result, type), quiet));
        }
        return e.pack(result);
    }};

    switch(op)
    {
        case code::v128_const:
        {
            if(bytes == nullptr) { return nullptr; }
            ::llvm::SmallVector<::llvm::Constant*, 16> constants{};
            for(unsigned i{}; i != 16u; ++i) { constants.push_back(b.getInt8(::std::to_integer<unsigned char>(bytes[i]))); }
            return e.pack(::llvm::ConstantVector::get(constants));
        }
        case code::v128_not: return b.CreateNot(a);
        case code::v128_and: return b.CreateAnd(a, c);
        case code::v128_andnot: return b.CreateAnd(a, b.CreateNot(c));
        case code::v128_or: return b.CreateOr(a, c);
        case code::v128_xor: return b.CreateXor(a, c);
        case code::v128_bitselect: return b.CreateOr(b.CreateAnd(a, d), b.CreateAnd(c, b.CreateNot(d)));
        case code::v128_any_true:
            return b.CreateZExt(e.intrinsic(::llvm::Intrinsic::vector_reduce_or,
                                b.CreateICmpNE(a, ::llvm::Constant::getNullValue(a->getType()))), b.getInt32Ty());
        case code::i8x16_shuffle:
        {
            if(bytes == nullptr) { return nullptr; }
            ::llvm::SmallVector<int, 16> mask{};
            for(unsigned i{}; i != 16u; ++i)
            {
                auto index{::std::to_integer<unsigned char>(bytes[i])};
                if(index >= 32u) { return nullptr; }
                mask.push_back(index);
            }
            return e.pack(b.CreateShuffleVector(e.lanes(a, 8u), e.lanes(c, 8u), mask));
        }
        case code::i8x16_swizzle:
            return e.pack(e.swizzle(e.lanes(a, 8u), e.lanes(c, 8u)));

#define UWVM_SIMD_INT_SHAPE(Prefix, Bits) \
        case code::Prefix##_splat: return e.pack(b.CreateVectorSplat(128u / Bits, b.CreateIntCast(a, b.getIntNTy(Bits), false))); \
        case code::Prefix##_replace_lane: return lane < 128u / Bits ? e.pack(b.CreateInsertElement(e.lanes(a, Bits), b.CreateIntCast(c, b.getIntNTy(Bits), false), b.getInt32(lane))) : nullptr; \
        case code::Prefix##_abs: { auto v{e.lanes(a, Bits)}; return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::abs, {v->getType()}, {v, b.getFalse()})); } \
        case code::Prefix##_neg: return e.pack(b.CreateNeg(e.lanes(a, Bits))); \
        case code::Prefix##_all_true: return e.test(a, Bits, false); \
        case code::Prefix##_bitmask: return e.test(a, Bits, true); \
        case code::Prefix##_shl: return e.pack(e.shift(a, c, Bits, 0u)); \
        case code::Prefix##_shr_s: return e.pack(e.shift(a, c, Bits, 1u)); \
        case code::Prefix##_shr_u: return e.pack(e.shift(a, c, Bits, 2u)); \
        case code::Prefix##_add: return binary(Bits, 0u); \
        case code::Prefix##_sub: return binary(Bits, 1u);
        UWVM_SIMD_INT_SHAPE(i8x16, 8u)
        UWVM_SIMD_INT_SHAPE(i16x8, 16u)
        UWVM_SIMD_INT_SHAPE(i32x4, 32u)
        UWVM_SIMD_INT_SHAPE(i64x2, 64u)
#undef UWVM_SIMD_INT_SHAPE
        case code::i16x8_mul: return binary(16u, 2u);
        case code::i32x4_mul: return binary(32u, 2u);
        case code::i64x2_mul: return binary(64u, 2u);
        case code::i8x16_popcnt: return unary_intrinsic(8u, ::llvm::Intrinsic::ctpop);

#define UWVM_SIMD_INT_COMPARE(Prefix, Bits) \
        case code::Prefix##_eq: return cmp(Bits, ::llvm::CmpInst::ICMP_EQ); \
        case code::Prefix##_ne: return cmp(Bits, ::llvm::CmpInst::ICMP_NE); \
        case code::Prefix##_lt_s: return cmp(Bits, ::llvm::CmpInst::ICMP_SLT); \
        case code::Prefix##_gt_s: return cmp(Bits, ::llvm::CmpInst::ICMP_SGT); \
        case code::Prefix##_le_s: return cmp(Bits, ::llvm::CmpInst::ICMP_SLE); \
        case code::Prefix##_ge_s: return cmp(Bits, ::llvm::CmpInst::ICMP_SGE);
        UWVM_SIMD_INT_COMPARE(i8x16, 8u)
        UWVM_SIMD_INT_COMPARE(i16x8, 16u)
        UWVM_SIMD_INT_COMPARE(i32x4, 32u)
        UWVM_SIMD_INT_COMPARE(i64x2, 64u)
#undef UWVM_SIMD_INT_COMPARE
#define UWVM_SIMD_INT_UNSIGNED_MINMAX(Prefix, Bits) \
        case code::Prefix##_lt_u: return cmp(Bits, ::llvm::CmpInst::ICMP_ULT); \
        case code::Prefix##_gt_u: return cmp(Bits, ::llvm::CmpInst::ICMP_UGT); \
        case code::Prefix##_le_u: return cmp(Bits, ::llvm::CmpInst::ICMP_ULE); \
        case code::Prefix##_ge_u: return cmp(Bits, ::llvm::CmpInst::ICMP_UGE); \
        case code::Prefix##_min_s: return bin_intrinsic(Bits, ::llvm::Intrinsic::smin); \
        case code::Prefix##_min_u: return bin_intrinsic(Bits, ::llvm::Intrinsic::umin); \
        case code::Prefix##_max_s: return bin_intrinsic(Bits, ::llvm::Intrinsic::smax); \
        case code::Prefix##_max_u: return bin_intrinsic(Bits, ::llvm::Intrinsic::umax);
        UWVM_SIMD_INT_UNSIGNED_MINMAX(i8x16, 8u)
        UWVM_SIMD_INT_UNSIGNED_MINMAX(i16x8, 16u)
        UWVM_SIMD_INT_UNSIGNED_MINMAX(i32x4, 32u)
#undef UWVM_SIMD_INT_UNSIGNED_MINMAX
#define UWVM_SIMD_SATURATING(Prefix, Bits) \
        case code::Prefix##_add_sat_s: return bin_intrinsic(Bits, ::llvm::Intrinsic::sadd_sat); \
        case code::Prefix##_add_sat_u: return bin_intrinsic(Bits, ::llvm::Intrinsic::uadd_sat); \
        case code::Prefix##_sub_sat_s: return bin_intrinsic(Bits, ::llvm::Intrinsic::ssub_sat); \
        case code::Prefix##_sub_sat_u: return bin_intrinsic(Bits, ::llvm::Intrinsic::usub_sat); \
        case code::Prefix##_avgr_u: { auto x{e.lanes(a, Bits)}; auto y{e.lanes(c, Bits)}; return e.pack(b.CreateSub(b.CreateOr(x, y), b.CreateLShr(b.CreateXor(x, y), e.number(x->getType(), 1u)))); }
        UWVM_SIMD_SATURATING(i8x16, 8u)
        UWVM_SIMD_SATURATING(i16x8, 16u)
#undef UWVM_SIMD_SATURATING
#define UWVM_SIMD_EXTEND(Prefix, Source, Bits) \
        case code::Prefix##_extend_low_##Source##_s: return e.pack(e.extend(a, Bits, false, true)); \
        case code::Prefix##_extend_high_##Source##_s: return e.pack(e.extend(a, Bits, true, true)); \
        case code::Prefix##_extend_low_##Source##_u: return e.pack(e.extend(a, Bits, false, false)); \
        case code::Prefix##_extend_high_##Source##_u: return e.pack(e.extend(a, Bits, true, false)); \
        case code::Prefix##_extmul_low_##Source##_s: return e.pack(b.CreateMul(e.extend(a, Bits, false, true), e.extend(c, Bits, false, true))); \
        case code::Prefix##_extmul_high_##Source##_s: return e.pack(b.CreateMul(e.extend(a, Bits, true, true), e.extend(c, Bits, true, true))); \
        case code::Prefix##_extmul_low_##Source##_u: return e.pack(b.CreateMul(e.extend(a, Bits, false, false), e.extend(c, Bits, false, false))); \
        case code::Prefix##_extmul_high_##Source##_u: return e.pack(b.CreateMul(e.extend(a, Bits, true, false), e.extend(c, Bits, true, false)));
        UWVM_SIMD_EXTEND(i16x8, i8x16, 8u)
        UWVM_SIMD_EXTEND(i32x4, i16x8, 16u)
        UWVM_SIMD_EXTEND(i64x2, i32x4, 32u)
#undef UWVM_SIMD_EXTEND
#define UWVM_SIMD_NARROW(Prefix, Source, Bits) \
        case code::Prefix##_narrow_##Source##_s: return e.pack(e.concat(e.narrow(a, Bits, true), e.narrow(c, Bits, true))); \
        case code::Prefix##_narrow_##Source##_u: return e.pack(e.concat(e.narrow(a, Bits, false), e.narrow(c, Bits, false)));
        UWVM_SIMD_NARROW(i8x16, i16x8, 8u)
        UWVM_SIMD_NARROW(i16x8, i32x4, 16u)
#undef UWVM_SIMD_NARROW
        case code::i16x8_extadd_pairwise_i8x16_s: return e.pack(e.pairwise(a, 8u, true));
        case code::i16x8_extadd_pairwise_i8x16_u: return e.pack(e.pairwise(a, 8u, false));
        case code::i32x4_extadd_pairwise_i16x8_s: return e.pack(e.pairwise(a, 16u, true));
        case code::i32x4_extadd_pairwise_i16x8_u: return e.pack(e.pairwise(a, 16u, false));
        case code::i32x4_dot_i16x8_s:
        {
            auto type{e.integer(32u, 8u)};
            auto product{b.CreateMul(b.CreateSExt(e.lanes(a, 16u), type), b.CreateSExt(e.lanes(c, 16u), type))};
            // INT16_MIN * INT16_MIN + INT16_MIN * INT16_MIN wraps to INT32_MIN, not poison.
            return e.pack(b.CreateAdd(e.slice(product, 0u, 4u, 2u), e.slice(product, 1u, 4u, 2u)));
        }
        case code::i16x8_q15mulr_sat_s:
        {
            auto type{e.integer(32u, 8u)};
            auto product{b.CreateMul(b.CreateSExt(e.lanes(a, 16u), type), b.CreateSExt(e.lanes(c, 16u), type))};
            auto rounded{b.CreateAShr(b.CreateAdd(product, e.number(type, 0x4000u)), e.number(type, 15u))};
            return e.pack(b.CreateTrunc(e.intrinsic(::llvm::Intrinsic::smin, rounded, e.number(type, 32767u)), e.integer(16u)));
        }

#define UWVM_SIMD_EXTRACT(Name, Bits, ResultBits, Signed) \
        case code::Name: return lane < 128u / Bits ? b.CreateIntCast(b.CreateExtractElement(e.lanes(a, Bits), b.getInt32(lane)), b.getIntNTy(ResultBits), Signed) : nullptr;
        UWVM_SIMD_EXTRACT(i8x16_extract_lane_s, 8u, 32u, true)
        UWVM_SIMD_EXTRACT(i8x16_extract_lane_u, 8u, 32u, false)
        UWVM_SIMD_EXTRACT(i16x8_extract_lane_s, 16u, 32u, true)
        UWVM_SIMD_EXTRACT(i16x8_extract_lane_u, 16u, 32u, false)
        UWVM_SIMD_EXTRACT(i32x4_extract_lane, 32u, 32u, false)
        UWVM_SIMD_EXTRACT(i64x2_extract_lane, 64u, 64u, false)
#undef UWVM_SIMD_EXTRACT
#define UWVM_SIMD_FLOAT(Prefix, Bits) \
        case code::Prefix##_splat: return e.pack(b.CreateVectorSplat(128u / Bits, a)); \
        case code::Prefix##_extract_lane: return lane < 128u / Bits ? b.CreateExtractElement(e.lanes(a, Bits, true), b.getInt32(lane)) : nullptr; \
        case code::Prefix##_replace_lane: return lane < 128u / Bits ? e.pack(b.CreateInsertElement(e.lanes(a, Bits, true), c, b.getInt32(lane))) : nullptr; \
        case code::Prefix##_eq: return cmp(Bits, ::llvm::CmpInst::FCMP_OEQ, true); \
        case code::Prefix##_ne: return cmp(Bits, ::llvm::CmpInst::FCMP_UNE, true); \
        case code::Prefix##_lt: return cmp(Bits, ::llvm::CmpInst::FCMP_OLT, true); \
        case code::Prefix##_gt: return cmp(Bits, ::llvm::CmpInst::FCMP_OGT, true); \
        case code::Prefix##_le: return cmp(Bits, ::llvm::CmpInst::FCMP_OLE, true); \
        case code::Prefix##_ge: return cmp(Bits, ::llvm::CmpInst::FCMP_OGE, true); \
        case code::Prefix##_abs: return e.float_sign(a, Bits, false); \
        case code::Prefix##_neg: return e.float_sign(a, Bits, true); \
        case code::Prefix##_sqrt: return unary_intrinsic(Bits, ::llvm::Intrinsic::sqrt, true); \
        case code::Prefix##_ceil: return unary_intrinsic(Bits, ::llvm::Intrinsic::ceil, true); \
        case code::Prefix##_floor: return unary_intrinsic(Bits, ::llvm::Intrinsic::floor, true); \
        case code::Prefix##_trunc: return unary_intrinsic(Bits, ::llvm::Intrinsic::trunc, true); \
        case code::Prefix##_nearest: return unary_intrinsic(Bits, ::llvm::Intrinsic::roundeven, true); \
        case code::Prefix##_add: return float_binary(Bits, 0u); \
        case code::Prefix##_sub: return float_binary(Bits, 1u); \
        case code::Prefix##_mul: return float_binary(Bits, 2u); \
        case code::Prefix##_div: return float_binary(Bits, 3u); \
        case code::Prefix##_min: return bin_intrinsic(Bits, ::llvm::Intrinsic::minimum, true); \
        case code::Prefix##_max: return bin_intrinsic(Bits, ::llvm::Intrinsic::maximum, true); \
        case code::Prefix##_pmin: { auto x{e.lanes(a, Bits, true)}; auto y{e.lanes(c, Bits, true)}; return e.pack(b.CreateSelect(b.CreateFCmpOLT(y, x), y, x)); } \
        case code::Prefix##_pmax: { auto x{e.lanes(a, Bits, true)}; auto y{e.lanes(c, Bits, true)}; return e.pack(b.CreateSelect(b.CreateFCmpOLT(x, y), y, x)); }
        UWVM_SIMD_FLOAT(f32x4, 32u)
        UWVM_SIMD_FLOAT(f64x2, 64u)
#undef UWVM_SIMD_FLOAT
        case code::i32x4_trunc_sat_f32x4_s: return e.pack(e.convert_sat(a, 32u, true));
        case code::i32x4_trunc_sat_f32x4_u: return e.pack(e.convert_sat(a, 32u, false));
        case code::i32x4_trunc_sat_f64x2_s_zero: return e.pack(e.convert_sat(a, 64u, true));
        case code::i32x4_trunc_sat_f64x2_u_zero: return e.pack(e.convert_sat(a, 64u, false));
        case code::f32x4_convert_i32x4_s: return e.pack(b.CreateSIToFP(e.lanes(a, 32u), e.floating(32u)));
        case code::f32x4_convert_i32x4_u: return e.pack(b.CreateUIToFP(e.lanes(a, 32u), e.floating(32u)));
        case code::f64x2_convert_low_i32x4_s:
        case code::f64x2_convert_low_i32x4_u:
        {
            auto raw{e.slice(e.lanes(a, 32u), 0u, 2u)};
            bool sign{op == code::f64x2_convert_low_i32x4_s};
            if(e.target().isMIPS() && e.has_feature("msa"))
            {
                auto value{e.target().isArch32Bit() ? e.widen_mips32_words(e.lanes(a, 32u), sign)
                                                   : b.CreateIntCast(raw, e.integer(64u), sign)};
                return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID(sign ? "llvm.mips.ffint.s.d" : "llvm.mips.ffint.u.d"), {}, {value}));
            }
            return e.pack(sign ? b.CreateSIToFP(raw, e.floating(64u)) : b.CreateUIToFP(raw, e.floating(64u)));
        }
        case code::f64x2_promote_low_f32x4:
        {
            auto value{e.lanes(a, 32u, true)};
            if(e.target().isLoongArch() && e.has_feature("lsx"))
            {
                return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.loongarch.lsx.vfcvtl.d.s"), {}, {value}));
            }
            if(e.target().isMIPS() && e.has_feature("msa"))
            {
                return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.mips.fexupr.d"), {}, {value}));
            }
            if(e.target().isPPC() && e.has_feature("vsx"))
            {
                // Generic constrained fpext can select xscvspdpn, which preserves sNaNs. The vector
                // arithmetic conversion quiets them and avoids two scalar conversions. Duplicate
                // each source lane so either endian's physical word selection sees the same pair.
                auto pairs{b.CreateShuffleVector(value, value, {0, 0, 1, 1})};
                return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.ppc.vsx.xvcvspdp"), {}, {pairs}));
            }
            auto low{e.slice(value, 0u, 2u)};
            if(e.target().isLoongArch())
            {
                ::llvm::Value* result{::llvm::PoisonValue::get(e.floating(64u))};
                for(unsigned i{}; i != 2u; ++i)
                {
                    result = b.CreateInsertElement(result, emit_llvm_float_promote(b, b.CreateExtractElement(low, b.getInt32(i))), b.getInt32(i));
                }
                return e.pack(result);
            }
            llvm_wasm_arithmetic_scope arithmetic_scope{b};
            auto result{b.CreateFPExt(low, e.floating(64u))};
            if(e.target().isPPC() && !e.has_feature("vsx"))
            {
                // Scalar PPC lfs promotion can preserve a signaling NaN; Wasm arithmetic must quiet it.
                result = b.CreateSelect(b.CreateFCmpUNO(low, low), ::llvm::ConstantFP::getQNaN(e.floating(64u)), result);
            }
            return e.pack(result);
        }
        case code::f32x4_demote_f64x2_zero:
        {
            if(e.target().isLoongArch() && e.has_feature("lsx"))
            {
                auto value{e.lanes(a, 64u, true)};
                // LSX packs the second operand in the low half, matching Wasm's two low result lanes.
                return e.pack(b.CreateIntrinsic(::llvm::Intrinsic::lookupIntrinsicID("llvm.loongarch.lsx.vfcvt.s.d"), {},
                                               {::llvm::Constant::getNullValue(value->getType()), value}));
            }
            if(e.target().isLoongArch() || e.target().isSPARC())
            {
                auto value{e.lanes(a, 64u, true)};
                ::llvm::Value* result{::llvm::Constant::getNullValue(e.floating(32u))};
                for(unsigned i{}; i != 2u; ++i)
                {
                    result = b.CreateInsertElement(result, emit_llvm_float_demote(b, b.CreateExtractElement(value, b.getInt32(i))), b.getInt32(i));
                }
                return e.pack(result);
            }
            llvm_wasm_arithmetic_scope arithmetic_scope{b};
            auto type{e.floating(32u, 2u)};
            return e.pack(e.concat(b.CreateFPTrunc(e.lanes(a, 64u, true), type), ::llvm::Constant::getNullValue(type)));
        }
        default: return nullptr; // Memory operators are emitted only after the common address/protection check.
    }
}

// Every load reads EXACTLY the Wasm access width; loadN_zero/splat/extend must not overread a page.
template <llvm_jit_simd_code Op>
[[nodiscard]] inline ::llvm::Value* emit_load(::llvm::IRBuilder<>& b, ::llvm::Value* pointer,
                                              ::llvm::Value* old = nullptr, unsigned lane = 0u) noexcept
{
    using code = llvm_jit_simd_code;
    emitter e{b};
    constexpr auto bytes{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    if constexpr(Op == code::v128_load32x2_s || Op == code::v128_load32x2_u)
    {
        if(e.target().isMIPS() && e.target().isArch32Bit() && e.has_feature("msa"))
        {
            auto load{b.CreateLoad(b.getInt64Ty(), pointer, "simd.memory.load")};
            load->setVolatile(true); load->setAlignment(::llvm::Align{1u});
            auto little{e.endian(load)};
            ::llvm::Value* words{::llvm::Constant::getNullValue(e.integer(32u))};
            words = b.CreateInsertElement(words, b.CreateTrunc(little, b.getInt32Ty()), b.getInt32(0u));
            words = b.CreateInsertElement(words, b.CreateTrunc(b.CreateLShr(little, b.getInt64(32u)), b.getInt32Ty()), b.getInt32(1u));
            return e.pack(e.widen_mips32_words(words, Op == code::v128_load32x2_s));
        }
    }
    unsigned bits{static_cast<unsigned>(bytes * 8uz)};
    ::llvm::Type* load_type{};
    if constexpr(Op == code::v128_load) { bits = 8u; load_type = e.integer(8u); }
    else if constexpr(Op == code::v128_load8x8_s || Op == code::v128_load8x8_u) { bits = 8u; load_type = e.integer(8u, 8u); }
    else if constexpr(Op == code::v128_load16x4_s || Op == code::v128_load16x4_u) { bits = 16u; load_type = e.integer(16u, 4u); }
    else if constexpr(Op == code::v128_load32x2_s || Op == code::v128_load32x2_u) { bits = 32u; load_type = e.integer(32u, 2u); }
    else { load_type = b.getIntNTy(bits); }
    auto load{b.CreateLoad(load_type, b.CreatePointerCast(pointer, get_llvm_pointer_type(load_type)), "simd.memory.load")};
    load->setAlignment(::llvm::Align{1u});
    load->setVolatile(true);
    auto value{e.endian(load)};
    if constexpr(Op == code::v128_load) { return e.pack(value); }
    else if constexpr(Op == code::v128_load8x8_s || Op == code::v128_load16x4_s || Op == code::v128_load32x2_s)
    { return e.pack(b.CreateSExt(value, e.integer(bits * 2u))); }
    else if constexpr(Op == code::v128_load8x8_u || Op == code::v128_load16x4_u || Op == code::v128_load32x2_u)
    { return e.pack(b.CreateZExt(value, e.integer(bits * 2u))); }
    else if constexpr(Op == code::v128_load8_splat || Op == code::v128_load16_splat || Op == code::v128_load32_splat || Op == code::v128_load64_splat)
    { return e.pack(b.CreateVectorSplat(128u / bits, value)); }
    else if constexpr(Op == code::v128_load32_zero || Op == code::v128_load64_zero)
    { return e.pack(b.CreateInsertElement(::llvm::Constant::getNullValue(e.integer(bits)), value, b.getInt32(0u))); }
    else
    { return old != nullptr && lane < 128u / bits ? e.pack(b.CreateInsertElement(e.lanes(old, bits), value, b.getInt32(lane))) : nullptr; }
}

template <llvm_jit_simd_code Op>
[[nodiscard]] inline ::llvm::StoreInst* emit_store(::llvm::IRBuilder<>& b, ::llvm::Value* pointer,
                                                   ::llvm::Value* raw, unsigned lane = 0u) noexcept
{
    emitter e{b};
    ::llvm::Value* value{};
    if constexpr(Op == llvm_jit_simd_code::v128_store) { value = e.lanes(raw, 8u); }
    else
    {
        constexpr auto bits{static_cast<unsigned>(llvm_jit_simd_details::simd_memory_access_size<Op>() * 8uz)};
        if(lane >= 128u / bits) { return nullptr; }
        value = e.endian(b.CreateExtractElement(e.lanes(raw, bits), b.getInt32(lane)));
    }
    return finalize_llvm_jit_direct_memory_store(b.CreateStore(value, b.CreatePointerCast(pointer, get_llvm_pointer_type(value->getType()))), ::llvm::Align{1u});
}
} // namespace simd_ir

inline void legalize_llvm_jit_native_vectors(::llvm::Module& module) noexcept
{
    // SPARC V8 has no native SIMD. LLVM 22 loops in narrow-vector extension legalization; expand IR operations
    // explicitly after optimization. This does not affect vector-capable targets or add runtime bridge calls.
    if(::llvm::Triple{module.getTargetTriple()}.getArch() != ::llvm::Triple::sparc) { return; }
    ::llvm::ScalarizerPassOptions options;
    options.ScalarizeLoadStore = true;
    // Unlike opt, an embedded JIT has not globally initialized legacy optimization passes. This also registers
    // the scalarizer's dependencies, using LLVM's once-only initialization before any parallel compilation.
    ::llvm::initializeScalarizerLegacyPassPass(*::llvm::PassRegistry::getPassRegistry());
    // LLVM 22 requests this analysis but omits it from Scalarizer's INITIALIZE_PASS_DEPENDENCY list.
    ::llvm::initializeTargetTransformInfoWrapperPassPass(*::llvm::PassRegistry::getPassRegistry());
    ::llvm::legacy::FunctionPassManager passes{::std::addressof(module)};
    passes.add(::llvm::createScalarizerPass(options));
    passes.doInitialization();
    for(auto& function: module) { if(!function.isDeclaration()) { passes.run(function); } }
    passes.doFinalization();
}

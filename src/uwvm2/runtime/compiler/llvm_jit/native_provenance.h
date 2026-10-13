/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <limits>
# if defined(__linux__) && (defined(__powerpc__) || defined(__loongarch__) || defined(__arm__))
#  include <sys/auxv.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <fast_io.h>
#  include <fast_io_dsal/string.h>
#  include <llvm/IR/DebugProgramInstruction.h>
#  include <llvm/ADT/SmallVector.h>
#  include <llvm/ADT/DenseMap.h>
#  include <llvm/ADT/ArrayRef.h>
#  include <llvm/BinaryFormat/Dwarf.h>
#  include <llvm/IR/Intrinsics.h>
#  include <llvm/IR/Instructions.h>
#  include <llvm/IR/InlineAsm.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/DIBuilder.h>
#  include <llvm/IR/DebugInfoMetadata.h>
#  include <llvm/IR/Function.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/Dominators.h>
#  include <llvm/IR/ValueHandle.h>
#  include <llvm/ADT/SmallPtrSet.h>
#  if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
#   include <llvm/ADT/StringMap.h>
#   include <llvm/TargetParser/Host.h>
#   include <llvm/TargetParser/X86TargetParser.h>
#  endif
# if defined(__linux__) && (defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4))
#  include <llvm/TargetParser/Host.h>
# endif
#  include <llvm/IR/Module.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::native_provenance
{
    // Native DWARF has a synthetic file, independently of guest .debug_*.
    // A nonzero native line encodes Wasm expression byte offset + 1; line zero
    // is deliberately unknown (entry scaffolding, wrappers and epilogues).
    // Optimization may merge, remove or reorder ranges. This metadata NEVER
    // grants guest-local, guest-memory or arbitrary native-address authority.
    inline constexpr unsigned format_version{1u};
    inline constexpr char producer[]{"uwvm debug-jit native Wasm provenance v1"};
    inline constexpr char directory[]{"uwvm-native-provenance-v1"};

    [[nodiscard]] inline bool require_flag(::llvm::Module& module, char const* name, unsigned value) noexcept
    {
        if(auto const existing{module.getModuleFlag(name)}; existing != nullptr)
        {
            auto const integer{::llvm::mdconst::dyn_extract<::llvm::ConstantInt>(existing)};
            return integer != nullptr && integer->getValue() == value;
        }
        module.addModuleFlag(::llvm::Module::Error, name, value);
        return true;
    }

    // Called only after the real full-only compilation admission. Metadata
    // belongs to the LLVM module/context; no DIBuilder or file-name borrow
    // survives this synchronous call and no runtime instructions are emitted.
    [[nodiscard]] inline ::llvm::DISubprogram* attach(::llvm::Function& function, ::llvm::StringRef identity_file) noexcept
    {
        auto* const module{function.getParent()};
        if(module == nullptr || identity_file.empty() || function.getSubprogram() != nullptr ||
           !require_flag(*module, "Debug Info Version", ::llvm::DEBUG_METADATA_VERSION) ||
           !require_flag(*module, "Dwarf Version", 5u) ||
           !require_flag(*module, "uwvm.native.provenance.version", format_version)) { return nullptr; }
        ::llvm::DIBuilder builder{*module};
        auto* const file{builder.createFile(identity_file, directory)};
        // This is a synthetic assembly/provenance line table, not a claim
        // that generated native stack slots implement C/C++/Rust variables.
        auto* const unit{builder.createCompileUnit(::llvm::dwarf::DW_LANG_Mips_Assembler,
            file, producer, true, "", 0u, "", ::llvm::DICompileUnit::FullDebug)};
        auto* const type{builder.createSubroutineType(builder.getOrCreateTypeArray({}))};
        auto* const scope{builder.createFunction(file, function.getName(), function.getName(), file,
            0u, type, 0u, ::llvm::DINode::FlagZero, ::llvm::DISubprogram::SPFlagDefinition)};
        if(unit == nullptr || scope == nullptr) { builder.finalize(); return nullptr; }
        function.setSubprogram(scope);
        // The native debugger cannot execute RuntimeDyld trampolines outside
        // this exact Wasm function. The qualified MIPS branch-expansion pass
        // retains short intra-function edges as PC-relative branches without
        // changing the static host-call ABI or long-branch range checks.
        // Other targets ignore this debug-only compiler attribute.
        function.addFnAttr("uwvm.native.debug.pcrel");
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
        // The public debugger protects EBP/RBP as frame infrastructure.
        // Reserve it in this debug-only body: register allocation can otherwise
        // put the only Wasm numeric ALU result there and hide every useful row.
        // Ordinary/null builds never attach this synthetic debug provenance.
        function.addFnAttr("frame-pointer", "all");
#endif
        builder.finalize();
        return scope;
    }

    [[nodiscard]] inline bool location(::llvm::IRBuilder<>& builder, ::llvm::DISubprogram* scope,
        ::std::size_t wasm_offset, ::std::size_t expression_size) noexcept
    {
        if(scope == nullptr || wasm_offset >= expression_size ||
           wasm_offset >= (::std::numeric_limits<unsigned>::max)()) { return false; }
        // [validated expression offset ... expression_size) end
        // [safe                                              ] bounded scalar only;
        //  ^^ offset+1 fits the LLVM line field, no byte cursor is read or advanced.
        builder.SetCurrentDebugLocation(::llvm::DILocation::get(builder.getContext(),
            static_cast<unsigned>(wasm_offset + 1u), 0u,
            ::llvm::DILexicalBlock::getDistinct(builder.getContext(), scope, scope->getFile(),
                static_cast<unsigned>(wasm_offset + 1u), 0u)));
        return true;
    }


#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
    [[nodiscard]] inline unsigned effective_x86_numeric_features(::llvm::IRBuilder<>& ir) noexcept
    {
        static unsigned const hardware{[]
        {
            auto features{::llvm::sys::getHostCPUFeatures()};
            return (features.lookup("sse") ? 1u : 0u) | (features.lookup("sse2") ? 2u : 0u);
        }()};
        auto* const function{ir.GetInsertBlock()->getParent()};
        auto const cpu{function->getFnAttribute("target-cpu")};
        auto const attributes{function->getFnAttribute("target-features")};
        if(!cpu.isStringAttribute() && !attributes.isStringAttribute()) { return hardware; }
        ::llvm::StringMap<bool> features{};
        features["sse"]=(hardware & 1u)!=0u;features["sse2"]=(hardware & 2u)!=0u;
        if(cpu.isStringAttribute() && !cpu.getValueAsString().empty())
        {
            ::llvm::SmallVector<::llvm::StringRef,64u> baseline{};
            auto const name{cpu.getValueAsString()};
            if(::llvm::X86::parseArchX86(name)==::llvm::X86::CK_None)
            { if(name!="generic") { return 0u; } }
            else { ::llvm::X86::getFeaturesForCPU(name,baseline); }
            features["sse"]=false;features["sse2"]=false;
            for(auto name:baseline) { features[name]=true; }
        }
        if(attributes.isStringAttribute())
        {
            auto remaining{attributes.getValueAsString()};
            while(!remaining.empty())
            {
                auto parts{remaining.split(',')};auto token{parts.first};remaining=parts.second;
                if(token.size()<2u || (token.front()!='+' && token.front()!='-')) { continue; }
                bool const enabled{token.front()=='+'};auto const name{token.drop_front()};
                features[name]=enabled;::llvm::X86::updateImpliedFeatures(name,enabled,features);
            }
        }
        return hardware & ((features.lookup("sse") ? 1u : 0u) | (features.lookup("sse2") ? 2u : 0u));
    }
#endif

#if defined(__linux__) && (defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4))
    [[nodiscard]] inline bool effective_native_32bit_i64_fp(::llvm::IRBuilder<>& ir) noexcept
    {
# if defined(__arm__) && (!defined(__ARM_FP) || (__ARM_FP & 8) == 0)
        return false;
# else
#  if defined(__arm__)
        bool const hardware{(::getauxval(AT_HWCAP) & (1ul << 6u)) != 0u}; // Linux HWCAP_VFP
#  else
        bool const hardware{(::getauxval(AT_HWCAP) & 0x08000000ul) != 0u}; // Linux PPC_FEATURE_HAS_FPU
#  endif
        if(!hardware) { return false; }
        auto* const function{ir.GetInsertBlock()->getParent()};
        auto const cpu{function->getFnAttribute("target-cpu")};
        static auto const host_cpu{::llvm::sys::getHostCPUName()};
        if(cpu.isStringAttribute() && !cpu.getValueAsString().empty() &&
           cpu.getValueAsString() != "generic" && cpu.getValueAsString() != host_cpu) { return false; }
        auto const attributes{function->getFnAttribute("target-features")};
        if(attributes.isStringAttribute())
        {
            ::llvm::SmallVector<::llvm::StringRef, 32u> features;
            attributes.getValueAsString().split(features, ',');
            for(auto feature: features)
            {
                if(feature == "+soft-float" || feature == "-fpregs" || feature == "-fpregs64" || feature == "-fp64")
                { return false; }
            }
        }
        return true;
# endif
    }
#endif

    // A cooperative host call may spill numeric operands. An empty tied
    // register identity makes the allocator materialize the actual typed Wasm
    // value after that call; it does not read a chosen register or emit guest
    // supplied asm. Only its exact register-only DWARF liveness can release
    // bits. Unsupported register classes keep the ordinary DWARF path.
    [[nodiscard]] inline ::llvm::Value* materialize_numeric_register([[maybe_unused]] ::llvm::IRBuilder<>& ir,
        ::llvm::Value* actual, ::llvm::Instruction** register_witness = nullptr) noexcept
    {
        if(register_witness != nullptr) { *register_witness = nullptr; }
#if defined(__linux__)
        if(actual == nullptr) { return nullptr; }
        auto* const type{actual->getType()};
# if defined(__i386__) || defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4)
        if(type->isIntegerTy(64u))
        {
#  if defined(__i386__)
            if((effective_x86_numeric_features(ir) & 2u)==0u) { return actual; }
#  else
            if(!effective_native_32bit_i64_fp(ir)) { return actual; }
#  endif
            // A qualified complete FP register holds this Wasm i64 bit pattern even though
            // an i686 GPR cannot. This debug-only identity performs no FP
            // arithmetic: both bitcasts and the tied FP identity preserve
            // every validated input bit, including NaN-shaped encodings.
            // Keep the ordinary split GPR/stack DWARF expressions refused.
            auto* const carrier{ir.CreateBitCast(actual, ir.getDoubleTy())};
            if(auto* cast{::llvm::dyn_cast<::llvm::Instruction>(carrier)}; cast != nullptr)
            { cast->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {})); }
            auto* const held{materialize_numeric_register(ir, carrier, register_witness)};
            if(held == nullptr) { return nullptr; }
            ::llvm::IRBuilderBase::InsertPointGuard point{ir};
            if(register_witness != nullptr && *register_witness != nullptr) { ir.SetInsertPoint(*register_witness); }
            auto* const result{ir.CreateBitCast(held, type)};
            if(auto* cast{::llvm::dyn_cast<::llvm::Instruction>(result)}; cast != nullptr)
            { cast->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {})); }
            return result;
        }
# endif
        char const* constraint{};
        if(type->isIntegerTy(32u) || (type->isIntegerTy(64u) && __SIZEOF_POINTER__ == 8))
        { constraint = "=r,0"; }
        [[maybe_unused]] bool const scalar_fp{type->isFloatTy() || type->isDoubleTy()};
        [[maybe_unused]] bool const vector128{type->isVectorTy() && type->getPrimitiveSizeInBits().getFixedValue() == 128u};
# if defined(__x86_64__) || defined(__i386__)
        if(scalar_fp || vector128)
        {
            auto const needed{type->isFloatTy() ? 1u : 2u};
            if((effective_x86_numeric_features(ir) & needed)==0u) { return actual; }
            constraint = "=x,0";
        }
# elif defined(__aarch64__)
        if(scalar_fp || vector128) { constraint = "=w,0"; }
# elif defined(__arm__)
        if(scalar_fp)
        { if(!effective_native_32bit_i64_fp(ir)) { return actual; } constraint = "=w,0"; }
#  if defined(__ARM_NEON)
        if(vector128) { constraint = "=w,0"; }
#  endif
# elif defined(__powerpc__)
        if(scalar_fp)
        {
#  if __SIZEOF_POINTER__ == 4
            if(!effective_native_32bit_i64_fp(ir)) { return actual; }
#  endif
            constraint = "=f,0";
        }
        if(vector128 && (::getauxval(AT_HWCAP) & 0x10000000ul) != 0u)
        {
            // The emitted native target uses actual Linux VMX capabilities.
            // A scalar C++ compiler baseline is not evidence that the running
            // process lacks a physical VMX class. Respect an explicit function
            // override; kernel capability alone never overrides -altivec.
            auto const attributes{ir.GetInsertBlock()->getParent()->getFnAttribute("target-features")};
            bool enabled{true};
            if(attributes.isStringAttribute())
            {
                enabled = false;
                ::llvm::SmallVector<::llvm::StringRef, 32u> features;
                attributes.getValueAsString().split(features, ',');
                for(auto feature: features)
                { if(feature.size() > 1u && feature.drop_front() == "altivec") { enabled = feature.front() == '+'; } }
            }
            if(enabled) { constraint = "=v,0"; }
        }
# elif defined(__loongarch__) && defined(__linux__)
        if(scalar_fp) { constraint = "=f,0"; }
        if(vector128 && (::getauxval(AT_HWCAP) & (1ul << 4u)) != 0u)
        {
            // Actual kernel LSX capability and the emitted function's effective
            // target feature govern a complete 128-bit witness, independently
            // of the baseline C++ compiler's optional-vector macros.
            auto const attributes{ir.GetInsertBlock()->getParent()->getFnAttribute("target-features")};
            bool enabled{true};
            if(attributes.isStringAttribute())
            {
                enabled = false;
                ::llvm::SmallVector<::llvm::StringRef, 32u> features;
                attributes.getValueAsString().split(features, ',');
                for(auto feature: features)
                { if(feature.size() > 1u && feature.drop_front() == "lsx") { enabled = feature.front() == '+'; } }
            }
            if(enabled) { constraint = "=f,0"; }
        }
# elif defined(__mips__) || defined(__loongarch__)
        if(scalar_fp) { constraint = "=f,0"; }
#  if defined(__mips_msa) || defined(__loongarch_sx)
        if(vector128) { constraint = "=f,0"; }
#  endif
# elif defined(__riscv)
#  if defined(__riscv_flen)
        if((type->isFloatTy() && __riscv_flen >= 32) || (type->isDoubleTy() && __riscv_flen >= 64))
        { constraint = "=f,0"; }
#  endif
# elif defined(__sparc__) || defined(__s390x__)
        if(scalar_fp) { constraint = "=f,0"; }
# endif
        if(constraint != nullptr)
        {
            ::llvm::Value* result{actual};
            // X86 uses a legal two-lane carrier for the complete 128 bits.
            // SelectionDAG loses empty vector INLINEASM outputs as DBG_VALUE
            // $noreg. A genuine vector XOR below produces a register-valued SSA
            // definition even when the original operand was spilled or constant.
            // This compiler-owned bitcast preserves every Wasm bit; no private
            // payload or arbitrary physical register is an input.
            auto* register_type{type};
            ::llvm::Value* register_input{actual};
# if defined(__x86_64__) || defined(__i386__)
            if(vector128)
            {
                register_type = ::llvm::FixedVectorType::get(ir.getInt64Ty(),2u);
                register_input = ir.CreateBitCast(actual,register_type);
                if(auto* cast{::llvm::dyn_cast<::llvm::Instruction>(register_input)};cast != nullptr)
                { cast->setMetadata("uwvm.native.numeric.code",::llvm::MDNode::get(ir.getContext(),{})); }
            }
# endif
            if(vector128)
            {
                // The compiler-owned tied scalar identity preserves this exact
                // zero, while keeping it opaque to LLVM's constant folding.
                // XOR with its zero splat preserves every input bit, including
                // NaN payloads. No synthetic value is exposed as a Wasm operand.
                auto* const zero_type{::llvm::FunctionType::get(ir.getInt32Ty(), {ir.getInt32Ty()}, false)};
                auto* const zero_asm{::llvm::InlineAsm::get(zero_type, "", "=r,0", true)};
                auto* const zero{ir.CreateCall(zero_type, zero_asm, {ir.getInt32(0u)})};
                zero->setDoesNotThrow();
                zero->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
                if(!register_type->getScalarType()->isIntegerTy())
                {
                    register_type = ::llvm::FixedVectorType::get(ir.getInt8Ty(), 16u);
                    register_input = ir.CreateBitCast(actual, register_type);
                }
                auto* const lane_zero{ir.CreateIntCast(zero, register_type->getScalarType(), false)};
                auto* const zero_vector{ir.CreateVectorSplat(
                    ::llvm::cast<::llvm::FixedVectorType>(register_type)->getNumElements(), lane_zero)};
                result = ir.CreateXor(register_input, zero_vector);
            }
            else
            {
                ::llvm::CallInst* scalar_result{};
# if defined(__powerpc64__)
                if(auto* constant{::llvm::dyn_cast<::llvm::ConstantInt>(register_input)};
                    constant != nullptr && constant->getType()->isIntegerTy(64u))
                {
                    // Actual validated Wasm constant, materialized using only
                    // immediate ALU instructions. No TOC or native DATA load
                    // can hide the consuming numeric witness from SI/NI.
                    auto const bits{constant->getZExtValue()};
                    auto const top{static_cast<int>((bits >> 48u) & 0xffffu)};
                    auto const signed_top{top < 32768 ? top : top - 65536};
                    auto const assembly{::fast_io::concat_fast_io("lis $0,", signed_top,
                        "\n\tori $0,$0,", (bits >> 32u) & 0xffffu,
                        "\n\tsldi $0,$0,32\n\toris $0,$0,", (bits >> 16u) & 0xffffu,
                        "\n\tori $0,$0,", bits & 0xffffu)};
                    auto* const signature{::llvm::FunctionType::get(register_type, false)};
                    auto* const identity{::llvm::InlineAsm::get(signature,
                        ::llvm::StringRef{assembly.data(),assembly.size()}, "=r", true)};
                    scalar_result = ir.CreateCall(signature,identity);
                }
                else
# endif
                {
                    auto* const signature{::llvm::FunctionType::get(register_type, {register_type}, false)};
                    auto* const identity{::llvm::InlineAsm::get(signature, "", constraint, true)};
                    scalar_result = ir.CreateCall(signature,identity,{register_input});
                }
                scalar_result->setDoesNotThrow();
                // This exact compiler-created empty tied identity consumes and
                // produces the same validated numeric bits. Its allocator copies
                // belong to that numeric value. Qualify its line as well as the
                // original definition: otherwise a zero-byte INLINEASM emits a
                // column-0 row at the next ALU address and conflicts with column 1.
                // No other inline assembly is traversed or qualified. The public
                // MC filter still refuses every load/store and SP/FP operand;
                // numeric register locations grant no spill or memory read.
                scalar_result->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
                result = scalar_result;
            }
            if(register_witness != nullptr)
            {
                auto const location{ir.getCurrentDebugLocation()};
                auto* const scope{location ? ::llvm::dyn_cast<::llvm::DILocalScope>(location.getScope()) : nullptr};
                if(scope != nullptr && location.getLine() != 0u)
                {
                    // A large immediate can take several machine instructions.
                    // A register-only DWARF location by itself can start during
                    // that expansion. This consuming marker has a distinct
                    // lexical range: the tied value is complete before its PC.
                    // It runs only in an explicitly enabled native debug plan.
                    auto* const marker_scope{::llvm::DILexicalBlock::getDistinct(ir.getContext(), scope,
                        scope->getFile(), location.getLine(), 0u)};
                    auto* const marker_type{::llvm::FunctionType::get(ir.getVoidTy(), {register_type}, false)};
                    // The output constraint starts with '=CLASS,0'; the marker
                    // consumes just CLASS, with no output or chosen register.
                    auto const input_constraint{::llvm::StringRef{constraint + 1u, 1u}};
                    auto* const marker_asm{::llvm::InlineAsm::get(marker_type, "nop", input_constraint, true)};
                    auto* const marker{ir.CreateCall(marker_type, marker_asm, {result})};
                    marker->setDoesNotThrow();
                    marker->setDebugLoc(::llvm::DILocation::get(ir.getContext(), location.getLine(), 1u, marker_scope));
                    marker->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
                    *register_witness = marker;
                }
            }
            return result;
        }
#endif
        return actual;
    }

    // Only actual, validated numeric Wasm SSA operands may call this producer.
    // Names/types are private synthetic metadata; guest DWARF is never input.
    inline bool numeric(::llvm::IRBuilder<>& ir, ::llvm::Value* value,
        unsigned kind, ::std::size_t ordinal, bool record_value = true,
        ::llvm::Instruction* register_witness = nullptr) noexcept
    {
        auto const location{register_witness == nullptr ? ir.getCurrentDebugLocation() : register_witness->getDebugLoc()};
        if(!location || location.getLine() == 0u || value == nullptr || ir.GetInsertBlock() == nullptr || ordinal >= 128u)
        { return false; }
        char const* name{}; unsigned bits{}, encoding{};
        switch(kind)
        {
            case 0x7fu: name = "uwvm.numeric.i32"; bits = 32u; encoding = ::llvm::dwarf::DW_ATE_unsigned; break;
            case 0x7eu: name = "uwvm.numeric.i64"; bits = 64u; encoding = ::llvm::dwarf::DW_ATE_unsigned; break;
            case 0x7du: name = "uwvm.numeric.f32"; bits = 32u; encoding = ::llvm::dwarf::DW_ATE_float; break;
            case 0x7cu: name = "uwvm.numeric.f64"; bits = 64u; encoding = ::llvm::dwarf::DW_ATE_float; break;
            case 0x7bu: name = "uwvm.numeric.v128"; bits = 128u; encoding = ::llvm::dwarf::DW_ATE_unsigned; break;
            default: return true; // references and private carriers have no numeric location
        }
        auto* const type{value->getType()};
        if((kind == 0x7fu && !type->isIntegerTy(32u)) || (kind == 0x7eu && !type->isIntegerTy(64u)) ||
           (kind == 0x7du && !type->isFloatTy()) || (kind == 0x7cu && !type->isDoubleTy()) ||
           (kind == 0x7bu && (!type->isVectorTy() || type->getPrimitiveSizeInBits().getFixedValue() != 128u))) { return false; }
        if(record_value)
        {
#if defined(__linux__) && defined(__arm__)
            // An ARM f32 S alias cannot justify a whole D location. If the
            // effective function cannot hold a complete D bit carrier, keep
            // its ordinary execution and publish no synthetic FP location.
            if(kind == 0x7du && !effective_native_32bit_i64_fp(ir)) { return true; }
#endif
            auto* const scope{::llvm::dyn_cast<::llvm::DILocalScope>(location.getScope())};
            if(scope == nullptr || !::llvm::isa<::llvm::DILexicalBlock>(scope)) { return false; }
            auto* const basic{::llvm::DIBasicType::get(ir.getContext(), ::llvm::dwarf::DW_TAG_base_type,
                name, bits, 0u, encoding, ::llvm::DINode::FlagZero)};
            auto const variable_name{::fast_io::concat_fast_io("uwvm.native.numeric.", ::fast_io::mnp::dec(ordinal))};
            auto* const variable{::llvm::DILocalVariable::get(ir.getContext(), scope,
                {variable_name.data(), variable_name.size()}, scope->getFile(), location.getLine(), basic,
                0u, ::llvm::DINode::FlagZero, 0u, {})};
            ::llvm::Value* register_value{value};
#if defined(__linux__) && (defined(__i386__) || defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4))
            if(kind == 0x7eu && register_witness != nullptr)
            {
                // Record the actual consumed complete FP carrier, rather than the
                // integer bitcast that legalization may split back into GPRs.
                // Only the exact compiler-created carrier/witness relation
                // qualifies this complete 64-bit location.
                auto* const cast{::llvm::dyn_cast<::llvm::BitCastInst>(value)};
                auto* const marker{::llvm::dyn_cast<::llvm::CallInst>(register_witness)};
                if(cast == nullptr || !cast->getOperand(0u)->getType()->isDoubleTy() ||
                   cast->getMetadata("uwvm.native.numeric.code") == nullptr || marker == nullptr ||
                   !marker->isInlineAsm() || marker->arg_size() != 1u ||
                   marker->getArgOperand(0u) != cast->getOperand(0u) ||
                   marker->getFunction() != cast->getFunction() ||
                   marker->getMetadata("uwvm.native.numeric.code") == nullptr) { return false; }
                register_value = cast->getOperand(0u);
            }
#endif
#if defined(__linux__) && defined(__powerpc64__)
            if(kind == 0x7fu)
            {
                // PPC64's i32 machine register is a subregister and produces a
                // DW_OP_bit_piece. Do not interpret pieces or read spill/stack
                // locations. Materialize this validated i32 as an explicit zext
                // into a complete X register instead. The synthetic variable and
                // public projection still qualify exactly its LOW 32 bits.
                ::llvm::IRBuilderBase::InsertPointGuard point{ir};
                if(register_witness != nullptr) { ir.SetInsertPoint(register_witness); }
                auto* const wide{ir.CreateZExt(value, ir.getInt64Ty())};
                if(auto* const cast{::llvm::dyn_cast<::llvm::Instruction>(wide)}; cast != nullptr)
                { cast->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {})); }
                register_value = materialize_numeric_register(ir, wide);
                if(register_value == nullptr) { return false; }
                if(register_witness != nullptr)
                {
                    // Keep the complete X value live at the actual consuming NOP,
                    // and define it BEFORE both the witness and its debug record.
                    // The old i32 operand cannot force a whole X register location.
                    auto* const marker_type{::llvm::FunctionType::get(ir.getVoidTy(), {ir.getInt64Ty()}, false)};
                    auto* const marker_asm{::llvm::InlineAsm::get(marker_type, "nop", "r", true)};
                    auto* const marker{ir.CreateCall(marker_type, marker_asm, {register_value})};
                    marker->setDoesNotThrow(); marker->setDebugLoc(register_witness->getDebugLoc());
                    marker->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
                    register_witness->eraseFromParent(); register_witness = marker;
                }
            }
#endif
#if defined(__linux__) && defined(__arm__)
            if(kind == 0x7du)
            {
                // ARM's two S subregisters share one DWARF D-register number.
                // Use a complete D bit carrier: the exact Wasm f32 occupies
                // its low half, and the other half is explicitly zero. An
                // ambiguous S alias cannot release unrelated host FP bits.
                ::llvm::IRBuilderBase::InsertPointGuard point{ir};
                if(register_witness != nullptr) { ir.SetInsertPoint(register_witness); }
                auto* const raw{ir.CreateBitCast(value, ir.getInt32Ty())};
                auto* const extended{ir.CreateZExt(raw, ir.getInt64Ty())};
                auto* const carrier{ir.CreateBitCast(extended, ir.getDoubleTy())};
                for(auto* actual: {raw, extended, carrier})
                {
                    if(auto* instruction{::llvm::dyn_cast<::llvm::Instruction>(actual)}; instruction != nullptr)
                    { instruction->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {})); }
                }
                register_value = materialize_numeric_register(ir, carrier);
                if(register_value == nullptr) { return false; }
                if(register_witness != nullptr)
                {
                    auto* const marker_type{::llvm::FunctionType::get(ir.getVoidTy(), {ir.getDoubleTy()}, false)};
                    auto* const marker_asm{::llvm::InlineAsm::get(marker_type, "nop", "w", true)};
                    auto* const marker{ir.CreateCall(marker_type, marker_asm, {register_value})};
                    marker->setDoesNotThrow(); marker->setDebugLoc(register_witness->getDebugLoc());
                    marker->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
                    register_witness->eraseFromParent(); register_witness = marker;
                }
            }
#endif
#if defined(__linux__) && defined(__s390x__)
            if(kind == 0x7du)
            {
                // SystemZ's F32 subregister is the high half of F64 and LLVM
                // encodes it with DW_OP_bit_piece. Keep the location reader
                // register-only: construct a complete F64 bit carrier with the
                // actual Wasm f32 in its high half and an explicit zero low
                // half. The public projection releases only the proved 32 bits.
                ::llvm::IRBuilderBase::InsertPointGuard point{ir};
                if(register_witness != nullptr) { ir.SetInsertPoint(register_witness); }
                auto* const raw{ir.CreateBitCast(value, ir.getInt32Ty())};
                auto* const extended{ir.CreateZExt(raw, ir.getInt64Ty())};
                auto* const shifted{ir.CreateShl(extended, ir.getInt64(32u))};
                auto* const carrier{ir.CreateBitCast(shifted, ir.getDoubleTy())};
                for(auto* actual: {raw, extended, shifted, carrier})
                {
                    if(auto* instruction{::llvm::dyn_cast<::llvm::Instruction>(actual)}; instruction != nullptr)
                    { instruction->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {})); }
                }
                register_value = materialize_numeric_register(ir, carrier);
                if(register_value == nullptr) { return false; }
                if(register_witness != nullptr)
                {
                    auto* const marker_type{::llvm::FunctionType::get(ir.getVoidTy(), {ir.getDoubleTy()}, false)};
                    auto* const marker_asm{::llvm::InlineAsm::get(marker_type, "nop", "f", true)};
                    auto* const marker{ir.CreateCall(marker_type, marker_asm, {register_value})};
                    marker->setDoesNotThrow(); marker->setDebugLoc(register_witness->getDebugLoc());
                    marker->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
                    register_witness->eraseFromParent(); register_witness = marker;
                }
            }
#endif
#if defined(__linux__) && defined(__powerpc64__)
            if((kind == 0x7eu || kind == 0x7bu) && register_witness != nullptr)
            {
                // Compiler-owned actual SSA operand. The stackmap records a
                // complete allocated register only at this shadow NOP's PC.
                // No stack slot, native pointer, or constant creates authority.
                ::llvm::IRBuilderBase::InsertPointGuard point{ir};
                ir.SetInsertPoint(register_witness);
                auto const id{0x5557000000000000ull | (static_cast<::std::uint64_t>(kind) << 40u) |
                    (static_cast<::std::uint64_t>(ordinal) << 32u) | location.getLine()};
                auto* const intrinsic{::llvm::Intrinsic::getOrInsertDeclaration(
                    ir.GetInsertBlock()->getModule(), ::llvm::Intrinsic::experimental_stackmap)};
                auto* const witness{ir.CreateCall(intrinsic, {ir.getInt64(id), ir.getInt32(4u), register_value})};
                witness->setDoesNotThrow(); witness->setDebugLoc(location);
                witness->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
            }
#endif
            auto* const record{::llvm::DbgVariableRecord::createDbgVariableRecord(register_value, variable,
                ::llvm::DIExpression::get(ir.getContext(), {}), location.get())};
            ir.GetInsertBlock()->insertDbgRecordBefore(record,
                register_witness == nullptr ? ir.GetInsertPoint() : register_witness->getIterator());
        }
        // Tag only pure numeric definitions reachable from the actual Wasm
        // value. Loads/calls/pointer arithmetic and runtime stores stay unknown.
        ::llvm::SmallVector<::llvm::Value*, 32u> pending{value};
        ::std::size_t visited{};
        while(!pending.empty() && visited++ != 4096u)
        {
            auto* const instruction{::llvm::dyn_cast<::llvm::Instruction>(pending.pop_back_val())};
            if(instruction == nullptr || instruction->getMetadata("uwvm.native.numeric.code") != nullptr ||
               instruction->getFunction() != ir.GetInsertBlock()->getParent()) { continue; }
            bool const pure{::llvm::isa<::llvm::BinaryOperator, ::llvm::CmpInst, ::llvm::SelectInst,
                ::llvm::PHINode, ::llvm::ExtractElementInst, ::llvm::InsertElementInst, ::llvm::ShuffleVectorInst>(instruction) ||
                (::llvm::isa<::llvm::CastInst>(instruction) && !::llvm::isa<::llvm::PtrToIntInst, ::llvm::IntToPtrInst>(instruction))};
            if(!pure || instruction->getType()->isPointerTy()) { continue; }
            bool pointer{};
            for(auto const& operand: instruction->operands()) { if(operand->getType()->isPointerTy()) { pointer = true; } }
            if(pointer) { continue; }
            instruction->setMetadata("uwvm.native.numeric.code", ::llvm::MDNode::get(ir.getContext(), {}));
            for(auto const& operand: instruction->operands()) { pending.push_back(operand.get()); }
        }
        return true;
    }
    // Compiler-owned transient identities, never serialized into guest or
    // global LLVM metadata. Tracking handles follow RAUW and become null when
    // a value is erased before finalization; no borrowed dead Value survives.
    struct numeric_identity
    {
        ::llvm::WeakTrackingVH input{}, definition{};
    };

    inline void rebind_numeric_identities(::llvm::Function& function,
        ::llvm::ArrayRef<numeric_identity> identities) noexcept
    {
        if(function.getSubprogram()==nullptr || identities.empty()) { return; }
        ::llvm::DominatorTree dominators{function};
        ::llvm::DenseMap<::llvm::Value*,::llvm::Value*> inputs{},roots{};
        for(auto const& identity:identities)
        {
            auto* original{static_cast<::llvm::Value*>(identity.input)};
            auto* definition{::llvm::dyn_cast_or_null<::llvm::Instruction>(static_cast<::llvm::Value*>(identity.definition))};
            if(original==nullptr || definition==nullptr || original==definition || definition->getFunction()!=&function ||
               original->getType()!=definition->getType() || !original->getType()->isVectorTy() ||
               original->getType()->getPrimitiveSizeInBits().getFixedValue()!=128u) { continue; }
            inputs[definition]=original;
        }
        // Resolve bit-preserving identity chains once, with path compression.
        // Tracking handles have already followed RAUW/deletion. A malformed
        // cycle cannot create a binding or a native value permission.
        auto const root{[&](::llvm::Value* value) -> ::llvm::Value*
        {
            ::llvm::SmallVector<::llvm::Value*,16u> path{};
            ::llvm::SmallPtrSet<::llvm::Value*,16u> visited{};
            auto* current{value};
            while(current!=nullptr)
            {
                auto cached{roots.find(current)};if(cached!=roots.end()) { current=cached->second;break; }
                auto found{inputs.find(current)};if(found==inputs.end()) { break; }
                if(!visited.insert(current).second) { current=nullptr;break; }
                path.push_back(current);current=found->second;
            }
            for(auto* item:path) { roots[item]=current; }
            return current;
        }};
        for(auto const& [definition,input]:inputs) { static_cast<void>(input);static_cast<void>(root(definition)); }
        ::llvm::DenseMap<::llvm::BasicBlock*,::llvm::SmallVector<::llvm::Use*,8u>> edges{};
        for(auto& block:function)
        {
            for(auto& phi:block.phis())
            { for(unsigned i{};i!=phi.getNumIncomingValues();++i) { edges[phi.getIncomingBlock(i)].push_back(&phi.getOperandUse(i)); } }
        }
        ::llvm::DenseMap<::llvm::Value*,::llvm::Instruction*> active{};
        struct change { ::llvm::Value* input{};::llvm::Instruction* previous{}; };
        ::llvm::SmallVector<change,32u> undo{};
        auto const rebind{[&](::llvm::Use& use)
        {
            auto* original{root(use.get())};if(original==nullptr) { return; }
            auto found{active.find(original)};
            if(found!=active.end() && found->second!=use.getUser() && dominators.dominates(found->second,use))
            { use.set(found->second); }
        }};
        struct frame { ::llvm::DomTreeNode* node{};::llvm::DomTreeNode::iterator next{},end{};::std::size_t restore{};bool entered{}; };
        ::llvm::SmallVector<frame,32u> pending{};
        if(auto* first{dominators.getRootNode()};first!=nullptr) { pending.push_back({first}); }
        // Iterative dominator-tree traversal: each operand and PHI edge is
        // visited once, and bindings are restored when leaving its subtree.
        // No sibling scan, repeated whole-constant use scan, or native stack
        // recursion depends on the size of an untrusted Wasm CFG.
        while(!pending.empty())
        {
            auto& item{pending.back()};
            if(!item.entered)
            {
                item.entered=true;item.restore=undo.size();item.next=item.node->begin();item.end=item.node->end();
                auto* block{item.node->getBlock()};
                for(auto& instruction:*block)
                {
                    if(!::llvm::isa<::llvm::PHINode>(instruction))
                    { for(auto& use:instruction.operands()) { rebind(use); } }
                    auto own{inputs.find(&instruction)};if(own==inputs.end()) { continue; }
                    auto* original{root(&instruction)};if(original==nullptr) { continue; }
                    auto previous{active.find(original)};undo.push_back({original,previous==active.end() ? nullptr : previous->second});
                    active[original]=&instruction;
                }
                auto outgoing{edges.find(block)};
                if(outgoing!=edges.end()) { for(auto* use:outgoing->second) { rebind(*use); } }
            }
            if(item.next!=item.end) { auto* child{*item.next};++item.next;pending.push_back({child});continue; }
            while(undo.size()!=item.restore)
            {
                auto previous{undo.pop_back_val()};
                if(previous.previous==nullptr) { active.erase(previous.input); }
                else { active[previous.input]=previous.previous; }
            }
            pending.pop_back();
        }
    }

    inline void restrict_public_code(::llvm::Function& function,
        ::llvm::ArrayRef<numeric_identity> identities = {}) noexcept
    {
        auto* const scope{function.getSubprogram()}; if(scope == nullptr) { return; }
        rebind_numeric_identities(function, identities);
        for(auto& block: function)
        {
            for(auto& instruction: block)
            {
                auto const location{instruction.getDebugLoc()};
                if(!location || location.getLine() == 0u)
                { instruction.setDebugLoc(::llvm::DILocation::get(function.getContext(), 0u, 0u, scope)); }
                else
                {
                    // Preserve the Wasm position map separately. Reserved
                    // synthetic column 1 qualifies pure numeric code display;
                    // column 0 grants no public native byte authority.
                    auto const numeric{instruction.getMetadata("uwvm.native.numeric.code") != nullptr};
                    instruction.setDebugLoc(::llvm::DILocation::get(function.getContext(), location.getLine(),
                        numeric ? 1u : 0u, location.getScope(), location.getInlinedAt()));
                }
            }
        }
    }

    inline void unknown(::llvm::IRBuilder<>& builder, ::llvm::DISubprogram* scope) noexcept
    {
        // Keep a scoped zero-line DebugLoc for inlinable calls in synthetic
        // scaffolding; an old opcode location must not leak into this region.
        builder.SetCurrentDebugLocation(scope == nullptr ? ::llvm::DebugLoc{} :
            ::llvm::DebugLoc{::llvm::DILocation::get(builder.getContext(), 0u, 0u, scope)});
    }
}
#endif

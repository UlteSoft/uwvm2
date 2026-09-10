#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory.h>

#include <atomic>
#include <bit>
#include <cstdint>

namespace
{
    namespace llvm_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    namespace int_details = ::uwvm2::runtime::compiler::uwvm_int::optable::details;

    using runtime_i32 = llvm_details::runtime_wasm_i32;

    [[nodiscard]] consteval runtime_i32 i32_bits(::std::uint32_t bits) noexcept
    { return ::std::bit_cast<runtime_i32>(bits); }

    consteval bool check_pure_effective_address_helpers() noexcept
    {
        auto const check_one{[](::std::uint32_t address_bits,
                                ::std::uint32_t static_offset,
                                ::std::uint_least64_t expected_offset,
                                bool expected_overflow) constexpr noexcept
                             {
                                 auto const address{i32_bits(address_bits)};
                                 auto const llvm_result{llvm_details::llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
                                 auto const int_result{int_details::wasm32_effective_offset(address, static_offset)};
                                 auto const stored_offset{sizeof(::std::size_t) >= sizeof(::std::uint_least64_t)
                                                              ? expected_offset
                                                              : static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(expected_offset))};
                                 return llvm_result.offset == stored_offset && llvm_result.offset_65_bit == expected_overflow &&
                                        int_result.offset == stored_offset && int_result.offset_65_bit == expected_overflow;
                             }};

        return check_one(0x80000000u, 0u, 0x80000000ull, false) &&
               check_one(0xffffffffu, 0u, 0xffffffffull, false) &&
               check_one(0xffffffffu, 1u, 0x100000000ull, true) &&
               check_one(0x80000000u, 0xffffffffu, 0x17fffffffull, true) &&
               check_one(0xffffffffu, 0xffffffffu, 0x1fffffffeull, true);
    }

    static_assert(check_pure_effective_address_helpers());

#if defined(UWVM_SUPPORT_MMAP)
    struct wasm32_full_protection_memory_mock
    {
        inline static constexpr bool can_mmap{true};
        ::std::atomic_size_t* memory_length_p{};
        ::uwvm2::object::memory::linear::mmap_memory_status_t status{
            ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32};

        [[nodiscard]] constexpr bool require_dynamic_determination_memory_size() const noexcept { return false; }
    };

    consteval bool check_full_protection_software_gate() noexcept
    {
        wasm32_full_protection_memory_mock memory{};
        if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
        {
            // These remain OOB Wasm accesses, but the full reservation now owns their complete native address range.
            // This helper decides whether to trap in software, not whether the access is logically valid. The mmap
            // guard-domain test separately requires actual hardware faults and checks the reported Wasm offset.
            return !int_details::should_trap_oob_unlocked(memory, int_details::wasm32_effective_offset(i32_bits(0xffffffffu), 1u), 1u) &&
                   !int_details::should_trap_oob_unlocked(memory, int_details::wasm32_effective_offset(i32_bits(0xffffffffu), 0xffffffffu), 64u) &&
                   int_details::should_trap_oob_unlocked(memory, {.offset = 0u, .offset_65_bit = true}, 1u) &&
                   !int_details::wasm32_full_mmap_can_use_hardware_bounds({.offset = 0u, .offset_65_bit = false}, 65u);
        }
        else
        {
            return int_details::should_trap_oob_unlocked(memory, int_details::wasm32_effective_offset(i32_bits(0xffffffffu), 1u), 1u);
        }
    }

    static_assert(check_full_protection_software_gate());
#endif

    [[nodiscard]] int check_direct_llvm_ir() noexcept
    {
        ::llvm::LLVMContext context{};
        ::llvm::Module module{"wasm32-effective-address", context};
        auto i32_type{::llvm::Type::getInt32Ty(context)};
        auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), {i32_type}, false)};
        auto function{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "probe", module)};
        auto block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};

        auto effective_offset{llvm_details::emit_llvm_wasm32_effective_offset(builder, function->getArg(0), 1u)};
        auto add{::llvm::dyn_cast_or_null<::llvm::BinaryOperator>(effective_offset)};
        if(add == nullptr || add->getOpcode() != ::llvm::Instruction::Add) { return 1; }
        if(!::llvm::isa<::llvm::ZExtInst>(add->getOperand(0)) || ::llvm::isa<::llvm::SExtInst>(add->getOperand(0))) { return 2; }

        auto out_of_range{llvm_details::emit_llvm_wasm32_effective_offset_out_of_range(builder, effective_offset)};
        auto range_compare{::llvm::dyn_cast_or_null<::llvm::ICmpInst>(out_of_range)};
        if(range_compare == nullptr || range_compare->getPredicate() != ::llvm::ICmpInst::ICMP_UGT) { return 3; }
        auto range_limit{::llvm::dyn_cast<::llvm::ConstantInt>(range_compare->getOperand(1))};
        if(range_limit == nullptr || range_limit->getZExtValue() != 0xffffffffull) { return 4; }

        // The relocatable mmap base must describe the whole externally reserved object, not a one-byte global. This
        // keeps a later non-inbounds GEP based on storage whose extent covers every checked or guard-protected access.
        constexpr ::std::size_t byte_span_size{4096uz};
        constexpr ::uwvm2::utils::container::u8string_view byte_span_symbol{u8"uwvm2test_memory_reserved_span"};
        auto byte_span_pointer{llvm_details::get_llvm_external_host_byte_span_pointer(
            builder, static_cast<::std::uintptr_t>(0x10000u), byte_span_size, byte_span_symbol)};
        if(byte_span_pointer == nullptr) { return 5; }
        if(auto global{::llvm::dyn_cast<::llvm::GlobalVariable>(byte_span_pointer)}; global != nullptr)
        {
            if(!global->isDeclaration()) { return 6; }
            auto array_type{::llvm::dyn_cast<::llvm::ArrayType>(global->getValueType())};
            if(array_type == nullptr || array_type->getElementType() != ::llvm::Type::getInt8Ty(context) ||
               array_type->getNumElements() != byte_span_size) { return 7; }
            if(llvm_details::get_llvm_external_host_byte_span_pointer(
                   builder, static_cast<::std::uintptr_t>(0x10000u), byte_span_size, byte_span_symbol) != byte_span_pointer) { return 8; }
            // A second declaration with an incompatible extent/type must fail before changing the process-global
            // symbol mapping. Reusing it would invalidate the storage/provenance contract of existing GEPs.
            if(llvm_details::get_llvm_external_host_byte_span_pointer(
                   builder, static_cast<::std::uintptr_t>(0x10000u), byte_span_size + 1uz, byte_span_symbol) != nullptr) { return 9; }
            if(llvm_details::get_llvm_external_host_object_pointer(
                   builder, static_cast<::std::uintptr_t>(0x10000u), ::llvm::Type::getInt8Ty(context), byte_span_symbol) != nullptr) { return 10; }

            constexpr ::uwvm2::utils::container::u8string_view defined_symbol{u8"uwvm2test_defined_memory_span"};
            auto defined_span_type{::llvm::ArrayType::get(::llvm::Type::getInt8Ty(context), 16u)};
            auto defined_span{new ::llvm::GlobalVariable(module,
                                                         defined_span_type,
                                                         false,
                                                         ::llvm::GlobalValue::ExternalLinkage,
                                                         ::llvm::ConstantAggregateZero::get(defined_span_type),
                                                         llvm_details::get_llvm_string_ref(defined_symbol))};
            if(defined_span == nullptr || llvm_details::get_llvm_external_host_object_pointer(
                                              builder,
                                              static_cast<::std::uintptr_t>(0x20000u),
                                              defined_span_type,
                                              defined_symbol) != nullptr) { return 11; }
        }

        auto memory_pointer{builder.CreateGEP(::llvm::Type::getInt8Ty(context), byte_span_pointer, effective_offset, "memory.addr")};
        auto memory_gep{::llvm::dyn_cast<::llvm::GetElementPtrInst>(memory_pointer)};
        if(memory_gep == nullptr || memory_gep->isInBounds()) { return 12; }

        auto store_slot{builder.CreateAlloca(i32_type)};
        auto store_inst{builder.CreateStore(::llvm::ConstantInt::get(i32_type, 1u), store_slot)};
        auto finalized_store{llvm_details::finalize_llvm_jit_direct_memory_store(store_inst, ::llvm::Align{1u})};
        if(finalized_store != store_inst || !store_inst->isVolatile() || store_inst->getAlign() != ::llvm::Align{1u}) { return 13; }

        builder.CreateRetVoid();
        return ::llvm::verifyModule(module, ::std::addressof(::llvm::errs())) ? 14 : 0;
    }
}

int main()
{
    return check_direct_llvm_ir();
}

#include <uwvm2/utils/macro/pop_macros.h>

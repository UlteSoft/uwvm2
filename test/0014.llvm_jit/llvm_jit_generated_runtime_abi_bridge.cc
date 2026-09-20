#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
    namespace jit_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    namespace wasm_type = ::uwvm2::uwvm::wasm::type;

    using raw_bridge_pointer = void (*)(::std::uintptr_t,
                                        ::std::uintptr_t,
                                        ::std::uintptr_t,
                                        ::std::size_t,
                                        ::std::uintptr_t,
                                        ::std::size_t) noexcept;
    using snapshot_bridge_pointer = ::std::uintptr_t (*)(::std::uintptr_t,
                                                          ::std::size_t,
                                                          ::std::size_t*) noexcept;

    static_assert(::std::is_same_v<decltype(&::uwvm2::runtime::lib::details::llvm_jit_call_raw_from_generated_wasm_abi_bridge),
                                   raw_bridge_pointer>);
    static_assert(::std::is_same_v<decltype(&jit_details::llvm_jit_local_imported_memory_snapshot_bridge), snapshot_bridge_pointer>);
    static_assert(sizeof(::std::size_t) == sizeof(::std::uintptr_t));
    static_assert(::std::numeric_limits<::std::size_t>::digits == ::std::numeric_limits<::std::uintptr_t>::digits);
    static_assert(jit_details::runtime_local_imported_page_size_is_representable(16u));
    static_assert(!jit_details::runtime_local_imported_page_size_is_representable(0u));
    static_assert(!jit_details::runtime_local_imported_page_size_is_representable(98304u));
    static_assert([]() constexpr noexcept
                  {
                      if constexpr(::std::numeric_limits<::std::size_t>::digits <
                                   ::std::numeric_limits<::std::uint_least64_t>::digits)
                      {
                          constexpr auto first_unrepresentable{
                              static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)()) + 1u};
                          return !jit_details::runtime_local_imported_page_size_is_representable(first_unrepresentable);
                      }
                      else { return true; }
                  }());

    inline bool snapshot_available{true};
    inline ::std::size_t reported_byte_length{32uz};

    struct snapshot_memory
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"memory"};
        inline static constexpr ::std::uint_least64_t page_size{16u};

        ::std::array<::std::byte, 32uz> bytes{};

        friend bool memory_grow(snapshot_memory&, ::std::uint_least64_t delta_pages) noexcept { return delta_pages == 0u; }
        friend ::std::byte* memory_begin(snapshot_memory& memory) noexcept { return memory.bytes.data(); }
        friend ::std::uint_least64_t memory_size(snapshot_memory&) noexcept { return 2u; }

        template <typename Function>
        friend bool with_memory_access_snapshot(snapshot_memory& memory, Function&& function) noexcept
        {
            if(!snapshot_available) { return false; }
            return static_cast<bool>(static_cast<Function&&>(function)(memory.bytes.data(), reported_byte_length));
        }
    };

    struct snapshot_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"generated-runtime-abi-probe"};
        using local_memory_tuple = ::uwvm2::utils::container::tuple<snapshot_memory>;
        local_memory_tuple local_memory{};
    };

    static_assert(wasm_type::is_local_imported_memory<snapshot_memory>);
    static_assert(wasm_type::is_local_imported_module<snapshot_module>);

    template <::std::uint_least64_t PageSize>
    struct explicit_page_size_memory
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"memory"};
        inline static constexpr ::std::uint_least64_t page_size{PageSize};

        ::std::byte byte{};

        friend bool memory_grow(explicit_page_size_memory&, ::std::uint_least64_t) noexcept { return false; }
        friend ::std::byte* memory_begin(explicit_page_size_memory& memory) noexcept { return ::std::addressof(memory.byte); }
        friend ::std::uint_least64_t memory_size(explicit_page_size_memory&) noexcept { return 1u; }

        template <typename Function>
        friend bool with_memory_access_snapshot(explicit_page_size_memory& memory, Function&& function) noexcept
        {
            return static_cast<bool>(static_cast<Function&&>(function)(
                ::std::addressof(memory.byte), static_cast<::std::size_t>(PageSize)));
        }
    };

    using zero_page_size_memory = explicit_page_size_memory<0u>;
    using non_power_of_two_page_size_memory = explicit_page_size_memory<98304u>;
    using four_gib_page_size_memory = explicit_page_size_memory<(::std::uint_least64_t{1u} << 32u)>;

    static_assert(wasm_type::declares_page_size<zero_page_size_memory>);
    static_assert(wasm_type::declares_page_size<non_power_of_two_page_size_memory>);
    static_assert(!wasm_type::has_page_size<zero_page_size_memory>);
    static_assert(!wasm_type::has_page_size<non_power_of_two_page_size_memory>);
    static_assert(!wasm_type::is_local_imported_memory<zero_page_size_memory>);
    static_assert(!wasm_type::is_local_imported_memory<non_power_of_two_page_size_memory>);
    static_assert(wasm_type::has_page_size<four_gib_page_size_memory> ==
                  (::std::numeric_limits<::std::size_t>::digits > 32u));
    static_assert(wasm_type::is_local_imported_memory<four_gib_page_size_memory> ==
                  (::std::numeric_limits<::std::size_t>::digits > 32u));

    [[nodiscard]] int fail(int line, char const* message) noexcept
    {
        ::std::fprintf(stderr, "llvm_jit_generated_runtime_abi_bridge:%d: %s\n", line, message);
        return 1;
    }

#define LLVM_GENERATED_ABI_REQUIRE(condition, message) \
    do                                                   \
    {                                                    \
        if(!(condition)) [[unlikely]]                    \
        {                                                \
            return fail(__LINE__, message);              \
        }                                                \
    } while(false)

    [[nodiscard]] bool has_narrow_extension_attribute(::llvm::CallInst const& call) noexcept
    {
        auto const attributes{call.getAttributes()};
        if(attributes.hasRetAttr(::llvm::Attribute::ZExt) || attributes.hasRetAttr(::llvm::Attribute::SExt)) { return true; }
        for(unsigned index{}; index != call.arg_size(); ++index)
        {
            if(attributes.hasParamAttr(index, ::llvm::Attribute::ZExt) || attributes.hasParamAttr(index, ::llvm::Attribute::SExt)) { return true; }
        }
        return false;
    }

    [[nodiscard]] int test_llvm_types_and_calls()
    {
        ::llvm::LLVMContext context{};
        ::llvm::Module module{"uwvm.generated-runtime-abi", context};
        ::llvm::IRBuilder<> builder{context};

        auto intptr_type{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
        auto raw_type{jit_details::get_llvm_runtime_raw_call_bridge_function_type(context)};
        LLVM_GENERATED_ABI_REQUIRE(raw_type != nullptr && raw_type->getReturnType()->isVoidTy() && !raw_type->isVarArg(),
                                   "raw bridge FunctionType return/varargs mismatch");
        LLVM_GENERATED_ABI_REQUIRE(raw_type->getNumParams() == 6u, "raw bridge FunctionType arity mismatch");
        for(auto parameter_type : raw_type->params())
        {
            LLVM_GENERATED_ABI_REQUIRE(parameter_type->isIntegerTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u)),
                                       "raw bridge operand is not register-wide intptr");
        }

        auto probe_type{::llvm::FunctionType::get(intptr_type, false)};
        auto probe{::llvm::Function::Create(probe_type, ::llvm::Function::ExternalLinkage, "abi.probe", module)};
        auto entry{::llvm::BasicBlock::Create(context, "entry", probe)};
        builder.SetInsertPoint(entry);

        auto raw_symbol{jit_details::get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_call_raw_from_generated_wasm_abi_bridge>(builder, raw_type)};
        LLVM_GENERATED_ABI_REQUIRE(raw_symbol != nullptr, "failed to materialize raw bridge declaration");

        ::std::array<::llvm::Value*, 6uz> raw_arguments{};
        for(auto& argument : raw_arguments) { argument = ::llvm::ConstantInt::get(intptr_type, 0u); }
        auto raw_call{jit_details::apply_llvm_jit_host_calling_conv(builder.CreateCall(raw_type, raw_symbol, raw_arguments))};
        LLVM_GENERATED_ABI_REQUIRE(raw_call != nullptr && raw_call->getFunctionType() == raw_type,
                                   "raw bridge call does not preserve the exact FunctionType");
        LLVM_GENERATED_ABI_REQUIRE(raw_call->getCallingConv() == jit_details::get_llvm_jit_host_calling_conv(),
                                   "raw bridge call uses the wrong calling convention");
        LLVM_GENERATED_ABI_REQUIRE(!has_narrow_extension_attribute(*raw_call),
                                   "raw bridge call unexpectedly carries a narrow extension attribute");

        auto pointer_type{jit_details::get_llvm_pointer_type(intptr_type)};
        auto snapshot_type{::llvm::FunctionType::get(intptr_type, {intptr_type, intptr_type, pointer_type}, false)};
        auto snapshot_symbol{jit_details::get_llvm_runtime_bridge_function_symbol_value<jit_details::llvm_jit_local_imported_memory_snapshot_bridge>(
            builder, snapshot_type)};
        LLVM_GENERATED_ABI_REQUIRE(snapshot_symbol != nullptr, "failed to materialize snapshot bridge declaration");

        auto byte_length_slot{builder.CreateAlloca(intptr_type)};
        auto snapshot_call{jit_details::apply_llvm_jit_host_calling_conv(builder.CreateCall(
            snapshot_type,
            snapshot_symbol,
             {::llvm::ConstantInt::get(intptr_type, 0u),
              ::llvm::ConstantInt::get(intptr_type, 0u),
             byte_length_slot}))};
        LLVM_GENERATED_ABI_REQUIRE(snapshot_call != nullptr && snapshot_call->getType() == intptr_type,
                                   "snapshot bridge return is not register-wide intptr");
        LLVM_GENERATED_ABI_REQUIRE(snapshot_call->getFunctionType() == snapshot_type,
                                   "snapshot bridge call does not preserve the exact FunctionType");
        LLVM_GENERATED_ABI_REQUIRE(snapshot_call->getCallingConv() == jit_details::get_llvm_jit_host_calling_conv(),
                                   "snapshot bridge call uses the wrong calling convention");
        LLVM_GENERATED_ABI_REQUIRE(!has_narrow_extension_attribute(*snapshot_call),
                                   "snapshot bridge call unexpectedly carries a narrow extension attribute");

        auto status_is_zero{builder.CreateICmpEQ(snapshot_call, ::llvm::ConstantInt::get(intptr_type, 0u))};
        auto compare{::llvm::dyn_cast<::llvm::ICmpInst>(status_is_zero)};
        LLVM_GENERATED_ABI_REQUIRE(compare != nullptr && compare->getPredicate() == ::llvm::ICmpInst::ICMP_EQ,
                                   "snapshot status is not compared with integer equality");
        LLVM_GENERATED_ABI_REQUIRE(compare->getOperand(0)->getType() == intptr_type && compare->getOperand(1)->getType() == intptr_type,
                                   "snapshot status comparison narrowed away from intptr");
        builder.CreateRet(snapshot_call);

        LLVM_GENERATED_ABI_REQUIRE(!::llvm::verifyModule(module, ::std::addressof(::llvm::errs())),
                                   "LLVM rejected the generated-runtime bridge ABI probe module");
        return 0;
    }

    [[nodiscard]] int test_snapshot_semantics()
    {
        wasm_type::local_imported_t provider{snapshot_module{}};
        auto const module_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(provider))};
        auto const bridge{&jit_details::llvm_jit_local_imported_memory_snapshot_bridge};

        constexpr ::std::size_t length_canary{0x2468u};
        ::std::size_t byte_length{length_canary};

        LLVM_GENERATED_ABI_REQUIRE(bridge(0u, 0uz, ::std::addressof(byte_length)) == 0u,
                                   "null module address did not return false");
        LLVM_GENERATED_ABI_REQUIRE(byte_length == length_canary, "null module address modified output canary");

        LLVM_GENERATED_ABI_REQUIRE(bridge(module_address, 0uz, nullptr) == 0u,
                                   "null length output did not return false");

        snapshot_available = false;
        LLVM_GENERATED_ABI_REQUIRE(bridge(module_address, 0uz, ::std::addressof(byte_length)) == 0u,
                                   "provider false status was not propagated");
        LLVM_GENERATED_ABI_REQUIRE(byte_length == length_canary, "provider false status modified output canary");

        snapshot_available = true;
        reported_byte_length = static_cast<::std::size_t>(snapshot_memory::page_size) + 1uz;
        LLVM_GENERATED_ABI_REQUIRE(bridge(module_address, 0uz, ::std::addressof(byte_length)) == 0u,
                                   "non-page-aligned provider snapshot did not fail");
        LLVM_GENERATED_ABI_REQUIRE(byte_length == length_canary, "invalid provider snapshot modified output canary");

        reported_byte_length = 32uz;
        LLVM_GENERATED_ABI_REQUIRE(bridge(module_address, 0uz, ::std::addressof(byte_length)) == 1u,
                                   "valid provider snapshot did not return true");
        LLVM_GENERATED_ABI_REQUIRE(byte_length == reported_byte_length, "valid provider snapshot wrote the wrong byte length");
        return 0;
    }
}  // namespace

int main()
{
    if(auto const result{test_llvm_types_and_calls()}; result != 0) { return result; }
    return test_snapshot_semantics();
}

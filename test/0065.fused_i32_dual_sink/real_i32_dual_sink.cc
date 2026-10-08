// Actual finalized parser/initializer -> ONE original INT fused walker ->
// original ring artifact and real LLVM SSA module. No pure body prevalidation,
// second body translator, fake stop/publication, or whole-tiered claim.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/shared/i32_dual_emission.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <type_traits>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT) || defined(UWVM2TEST_STRICT_NO_INTERPRETER)
#error "This fixture must execute the actual original INT artifact, not a replacement JIT test runner."
#endif
#if defined(UWVM_DISABLE_INT) || defined(UWVM_DISABLE_JIT)
#error "Use matching actual INT and LLVM compiler/runtime/provider source closure."
#endif
namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    namespace dual = ::uwvm2::runtime::compiler::shared::i32_dual_emission;
    namespace full = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
#if defined(UWVM2TEST_DUAL_I32_TAIL)
    constexpr auto option{k_test_tail_min_opt};
#else
    constexpr auto option{k_test_byref_opt};
#endif
    template<typename> struct first_argument;
    template<typename R, typename C, typename A>
    struct first_argument<R (C::*)(A)> { using type = ::std::remove_cvref_t<A>; };
    using native_symbol_name = typename first_argument<decltype(&::llvm::ExecutionEngine::getFunctionAddress)>::type;
    void require(bool valid, char const* text)
    {
        if(!valid)
        {
            ::fast_io::print(::fast_io::err(), "real fused i32 dual sink: ", ::fast_io::mnp::os_c_str(text), "\n");
            ::fast_io::fast_terminate();
        }
    }
    byte_vec source_file(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz, "bounded complete actual Wasm source");
        byte_vec source(input.size());
        // [RAII file input ... equal-length owned source] source_end
        // [safe] checked sizes BEFORE exact copy; neither pointer is advanced.
        ::fast_io::freestanding::my_memcpy(source.data(), input.data(), input.size());
        return source;
    }
    auto enabled_features()
    {
        auto result{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        policy.disable_gc = false; policy.disable_memory64 = false;
        policy.disable_function_references = false; policy.disable_exceptions = false; policy.disable_tail_call = false;
        return result;
    }
    ::std::uint32_t unpack_result(byte_vec const& result)
    {
        static_assert(sizeof(::std::uint32_t) == 4uz);
        require(result.size() == sizeof(::std::uint32_t), "actual tight i32 ring result ABI extent");
        ::std::uint32_t bits{};
        // [actual four result bytes] one-past
        // [safe] exact result size BEFORE bit-preserving ABI copy, not parsing.
        ::fast_io::freestanding::my_memcpy(::std::addressof(bits), result.data(), sizeof(bits));
        return bits;
    }
    ::std::uint32_t run_ring(dual::checked_module<option> const& owned, runtime_module_t const& module,
                             ::std::size_t index, byte_vec const& parameters = {})
    {
        require(index < module.local_defined_function_vec_storage.size() &&
            index < owned.ring_artifact().local_funcs.size(), "same actual local/ring index bounds");
        auto result{interpreter_runner<option>::run(owned.ring_artifact().local_funcs.index_unchecked(index),
            module.local_defined_function_vec_storage.index_unchecked(index), parameters, nullptr, nullptr)};
        return unpack_result(result.results);
    }
    ::std::uint32_t run_native(dual::checked_module<option>& owned, runtime_module_t const& module,
                               ::std::size_t index, bool parameter = false)
    {
        require(owned.native_ir_available(index), "actual same-walk LLVM owner is available");
        auto fragment{owned.take_checked_ir(index)};
        require(fragment.emitted && fragment.llvm_module && fragment.llvm_context_holder &&
            !owned.native_ir_available(index), "physical LLVM ownership moves once, source slot loses availability");
        auto const name{full::details::get_llvm_wasm_function_name(module, static_cast<::std::uint_least32_t>(index))};
        auto const definition{fragment.llvm_module->getFunction(full::details::get_llvm_string_ref(name))};
        require(definition && !definition->isDeclaration() &&
            definition->getCallingConv() == full::details::get_llvm_jit_wasm_calling_conv(), "actual original typed LLVM ABI");
        require(full::details::verify_llvm_jit_module(*fragment.llvm_module, true), "mandatory actual final LLVM module verifier");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine) && machine->createDataLayout() == fragment.llvm_module->getDataLayout(),
            "actual native target matches retained emission layout");
#if LLVM_VERSION_MAJOR >= 21
        require(machine->getTargetTriple() == fragment.llvm_module->getTargetTriple(), "actual LLVM target triple");
#else
        require(machine->getTargetTriple().str() == fragment.llvm_module->getTargetTriple(), "actual LLVM target triple");
#endif
        // The real engine dies BEFORE the context. The runtime/source outlives
        // both. This test resolves actual definitions, not arbitrary PC memory.
        auto context{::std::move(fragment.llvm_context_holder)};
        auto emitted{::std::move(fragment.llvm_module)};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{
            ::std::unique_ptr<::llvm::Module>{emitted.release()}}.setEngineKind(::llvm::EngineKind::JIT).create(machine.release())};
        require(bool(engine), "real MCJIT engine from original one-walk physical SSA module");
        engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "actual owned symbol size before end iterator formation");
        // [owned name.data() ... name.size() characters] one-past
        // [safe] checked extent BEFORE forming LLVM's actual std::string ABI name.
        native_symbol_name native_name{name.data(), name.data() + name.size()};
        auto const address{engine->getFunctionAddress(native_name)};
        require(address != 0u && address <= (::std::numeric_limits<::std::uintptr_t>::max)(), "actual finalized native typed entry");
        if(parameter)
        {
            using entry_type = ::std::uint32_t (UWVM2TEST_WASM_ABI*)(::std::uint32_t);
            return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(0x80000000u);
        }
        using entry_type = ::std::uint32_t (UWVM2TEST_WASM_ABI*)();
        return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))();
    }
    void positive(char const* path)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-i32-dual-sink", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 24uz,
            "actual all18 numeric plus initialized/local/mixed modern function set");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "qualified actual target is retained through synchronous emission");
        dual::emission_policy policy{}; policy.target = machine.get();
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto owned{dual::compile<option>(*prepared.mod, {}, policy, error, &features)};
        require(owned.typed_admission_complete() && error.err_code == error_code::ok &&
            owned.ring_artifact().local_funcs.size() == 24uz && owned.diagnostics().size() == 24uz,
            "ALL local functions original fused validated/fixed before factory seal");
        auto moved{::std::move(owned)};
        require(!owned.typed_admission_complete() && moved.typed_admission_complete(), "moved-from compiler owner cannot retain a seal");
        constexpr ::std::array<::std::uint32_t, 18uz> expected{
            32u, 32u, 32u, 0u, 0x7fffffffu, 42u, 42u, 0x55555555u, 0u,
            1u, 17u, 119u, 102u, 2u, 0xc0000000u, 0x40000000u, 3u, 0xc0000000u};
        for(::std::size_t i{}; i != expected.size(); ++i)
        {
            auto const local{i + 1uz}; // <=18; bounds checked by both real runners.
            require(moved.diagnostics()[local].reason == dual::native_unavailability::none &&
                moved.diagnostics()[local].i32_numeric_transitions == 1uz,
                "one actual shared typed transition per original numeric instruction");
            require(run_ring(moved, *prepared.mod, local) == expected[i], "original ring exact wrap/zero/shift/division/remainder bits");
            require(run_native(moved, *prepared.mod, local) == expected[i], "real same-walk LLVM exact numeric edge bits");
        }
        auto const argument{pack_i32((::std::numeric_limits<::std::int32_t>::min)())};
        require(run_ring(moved, *prepared.mod, 19uz, argument) == 0x80000000u &&
            run_native(moved, *prepared.mod, 19uz, true) == 0x80000000u,
            "actual local.get parameter SSA/ABI and original ring match");
        require(run_ring(moved, *prepared.mod, 20uz) == 0u && run_native(moved, *prepared.mod, 20uz) == 0u,
            "actual default-initialized i32 local owner matches");
        for(::std::size_t local{21uz}; local != 24uz; ++local)
        {
            require(!moved.native_ir_available(local) && moved.diagnostics()[local].reason != dual::native_unavailability::none &&
                run_ring(moved, *prepared.mod, local) == 42u,
                "legal mixed/nop/modern-i31/skipped constants explicitly LLVM-unavailable, ring remains executable");
        }
        auto quota_policy{policy}; quota_policy.module_operations = 0uz;
        auto quota{dual::compile<option>(*prepared.mod, {}, quota_policy, error, &features)};
        require(quota.typed_admission_complete() && !quota.native_ir_available(1uz) &&
            quota.diagnostics()[1uz].reason == dual::native_unavailability::operation_quota &&
            run_ring(quota, *prepared.mod, 6uz) == 42u, "zero IR quota never falsely invalidates legal guest/ring");
        auto function_quota_policy{policy}; function_quota_policy.module_functions = 0uz;
        auto function_quota{dual::compile<option>(*prepared.mod, {}, function_quota_policy, error, &features)};
        require(function_quota.typed_admission_complete() && function_quota.diagnostics().empty() &&
            !function_quota.native_ir_available(1uz) && run_ring(function_quota, *prepared.mod, 6uz) == 42u,
            "zero retained-function quota bounds optional IR owners, does not skip typed admission");
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_I32 all18=", expected.size(),
            " local_get=2 modern_i31_ring=1 unused_admission=all actual_llvm_and_ring_results=matched\n");
    }
    void unused_negative(char const* path, error_code expected)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-i32-dual-unused-invalid", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 2uz,
            "actual empty start followed by genuinely unused modern invalid body");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual optional native target");
        dual::emission_policy policy{}; policy.target = machine.get();
        ::uwvm2::validation::error::code_validation_error_impl error{};
        bool rejected{};
        try { auto refused{dual::compile<option>(*prepared.mod, {}, policy, error, &features)}; (void)refused; }
        catch(::fast_io::error const& actual)
        {
            if(actual.domain != ::fast_io::parse_domain_value || actual.code !=
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            rejected = true;
        }
        auto const code{prepared.mod->local_defined_function_vec_storage.index_unchecked(1uz).wasm_code_ptr};
        require(code != nullptr, "actual invalid function source metadata");
        auto const begin{reinterpret_cast<::std::byte const*>(code->body.expr_begin)};
        auto const end{reinterpret_cast<::std::byte const*>(code->body.code_end)};
        require(rejected && error.err_code == expected && error.err_curr >= begin && error.err_curr < end,
            "specific first-walk modern unused-body diagnostic inside the actual second expression");
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_I32 unused_invalid_diagnostic=", static_cast<unsigned>(expected), " accepted=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 4, "positive and two official parsed modern unused-invalid Wasm files");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual LLVM native target initialization");
    install_unexpected_traps();
    positive(argv[1]);
    unused_negative(argv[2], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[3], error_code::numeric_operand_type_mismatch);
}

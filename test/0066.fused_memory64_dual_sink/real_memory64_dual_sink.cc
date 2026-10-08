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
#if defined(UWVM2TEST_DUAL_MEMORY_CACHED_RING)
    constexpr auto option{make_tailcall_fully_split_opt<3uz, 3uz, 8uz, 8uz>()};
    static_assert(option.is_tail_call && option.i32_stack_top_begin_pos != option.i32_stack_top_end_pos &&
        option.i64_stack_top_begin_pos != option.i64_stack_top_end_pos &&
        option.i32_stack_top_begin_pos != SIZE_MAX && option.i64_stack_top_begin_pos != SIZE_MAX);
#elif defined(UWVM2TEST_DUAL_I32_TAIL)
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
            ::fast_io::print(::fast_io::err(), "real fused memory64 dual sink: ", ::fast_io::mnp::os_c_str(text), "\n");
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
        policy.disable_gc = false; policy.disable_memory64 = false; policy.disable_multi_memory = false;
        policy.disable_function_references = false; policy.disable_exceptions = false; policy.disable_tail_call = false;
        return result;
    }
    template<bool Wide>
    ::std::uint64_t run_ring(dual::checked_module<option> const& owned, runtime_module_t const& module,
                            ::std::size_t index, byte_vec const& parameters = {})
    {
        require(index < module.local_defined_function_vec_storage.size() &&
            index < owned.ring_artifact().local_funcs.size(), "same actual local/ring index bounds");
        auto result{interpreter_runner<option>::run(owned.ring_artifact().local_funcs.index_unchecked(index),
            module.local_defined_function_vec_storage.index_unchecked(index), parameters, nullptr, nullptr)};
        using bits_type = ::std::conditional_t<Wide, ::std::uint64_t, ::std::uint32_t>;
        require(result.results.size() == sizeof(bits_type), "actual tight original ring result ABI extent");
        bits_type bits{};
        // [actual complete result bytes] one-past
        // [safe] exact same ABI extent BEFORE bit-preserving copy, no endian parser.
        ::fast_io::freestanding::my_memcpy(::std::addressof(bits), result.results.data(), sizeof(bits));
        return bits;
    }
    template<bool Wide>
    ::std::uint64_t run_native(dual::checked_module<option>& owned, runtime_module_t const& module,
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
        using bits_type = ::std::conditional_t<Wide, ::std::uint64_t, ::std::uint32_t>;
        if(parameter)
        {
            using entry_type = bits_type (UWVM2TEST_WASM_ABI*)(bits_type);
            return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(bits_type{0x80000000u});
        }
        using entry_type = bits_type (UWVM2TEST_WASM_ABI*)();
        return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))();
    }
    void positive(char const* path)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-memory64-dual-sink", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 27uz,
            "actual twelve integer loads, seven stores, i64 locals and mixed modern functions");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual target is retained through synchronous physical emission");
        dual::emission_policy policy{}; policy.target = machine.get(); policy.emit_integer_scalar_memory = true;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto owned{dual::compile<option>(*prepared.mod, {}, policy, error, &features)};
        require(owned.typed_admission_complete() && error.err_code == error_code::ok &&
            owned.ring_artifact().local_funcs.size() == 27uz && owned.diagnostics().size() == 27uz,
            "ALL original typed function walks and ring fixups precede factory seal");
        struct expected_load { bool wide; ::std::uint64_t bits; };
        constexpr ::std::array<expected_load, 12uz> loads{{
            {false,0xbadcfe80ull}, {true,0x32547698badcfe80ull},
            {false,0xffffff80ull}, {false,0x80ull}, {false,0xfffffe80ull}, {false,0xfe80ull},
            {true,0xffffffffffffff80ull}, {true,0x80ull}, {true,0xfffffffffffffe80ull}, {true,0xfe80ull},
            {true,0xffffffffbadcfe80ull}, {true,0xbadcfe80ull}}};
        for(::std::size_t i{}; i != loads.size(); ++i)
        {
            auto const local{i + 1uz}; // <=12; bounds rechecked by both actual runners.
            auto const expected{loads[i]};
            require(owned.diagnostics()[local].reason == dual::native_unavailability::none &&
                owned.diagnostics()[local].integer_memory_transitions == 1uz,
                "one actual first-walk scalar typed event per integer load");
            if(expected.wide)
            { require(run_ring<true>(owned,*prepared.mod,local) == expected.bits &&
                run_native<true>(owned,*prepared.mod,local) == expected.bits, "actual i64 signed/unsigned LE load matches ring"); }
            else
            { require(run_ring<false>(owned,*prepared.mod,local) == expected.bits &&
                run_native<false>(owned,*prepared.mod,local) == expected.bits, "actual i32 signed/unsigned LE load matches ring"); }
        }
        constexpr ::std::array<expected_load, 7uz> stores{{
            {false,0xbadcfe80ull}, {true,0x32547698badcfe80ull}, {false,0x80ull}, {false,0xfe80ull},
            {true,0x80ull}, {true,0xfe80ull}, {true,0xbadcfe80ull}}};
        for(::std::size_t i{}; i != stores.size(); ++i)
        {
            auto const local{i + 13uz}; // <=19, exact original actual declaration order.
            auto const expected{stores[i]};
            require(owned.diagnostics()[local].integer_memory_transitions == 2uz,
                "original memory64 store consumes address/value, then real typed load consumes address");
            if(expected.wide)
            { require(run_ring<true>(owned,*prepared.mod,local) == expected.bits &&
                run_native<true>(owned,*prepared.mod,local) == expected.bits, "actual i64 truncating store+load matches ring"); }
            else
            { require(run_ring<false>(owned,*prepared.mod,local) == expected.bits &&
                run_native<false>(owned,*prepared.mod,local) == expected.bits, "actual 12-byte i64-address/i32-value store matches ring"); }
        }
        auto const argument{pack_i64(0x80000000ll)};
        require(run_ring<true>(owned,*prepared.mod,20uz,argument) == 0x80000000ull &&
            run_native<true>(owned,*prepared.mod,20uz,true) == 0x80000000ull &&
            run_ring<true>(owned,*prepared.mod,21uz) == 0ull && run_native<true>(owned,*prepared.mod,21uz) == 0ull,
            "actual i64 local parameter/default initialization SSA matches original ring");
        require(!owned.native_ir_available(22uz) && !owned.native_ir_available(23uz) && !owned.native_ir_available(26uz) &&
            run_ring<false>(owned,*prepared.mod,23uz) == 42u && run_ring<false>(owned,*prepared.mod,26uz) == 42u,
            "legal floating/mixed i31/polymorphic body remains typed valid, optional LLVM explicitly unavailable");
        require(run_ring<false>(owned,*prepared.mod,24uz) == 42u && run_native<false>(owned,*prepared.mod,24uz) == 42u &&
            run_ring<false>(owned,*prepared.mod,25uz) == 0x32u && run_native<false>(owned,*prepared.mod,25uz) == 0x32u,
            "actual selected memory32 index and nonzero memory64 offset preserve first decode identity");
        auto old_policy{policy}; old_policy.emit_integer_scalar_memory = false;
        auto old_slice{dual::compile<option>(*prepared.mod, {}, old_policy, error, &features)};
        require(old_slice.typed_admission_complete() && !old_slice.native_ir_available(1uz) &&
            !old_slice.native_ir_available(20uz) && run_ring<false>(old_slice,*prepared.mod,1uz) == 0xbadcfe80u,
            "default i32-only selected slice still declines extension without guest validity loss");
        auto quota_policy{policy}; quota_policy.module_operations = 0uz;
        auto quota{dual::compile<option>(*prepared.mod, {}, quota_policy, error, &features)};
        require(quota.typed_admission_complete() && !quota.native_ir_available(1uz) &&
            run_ring<false>(quota,*prepared.mod,1uz) == 0xbadcfe80u,
            "zero optional IR quota does not skip any original typed body admission");
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_MEMORY64 integer_loads=",loads.size(),
            " integer_stores=",stores.size()," locals64=2 selected_memory32=1 staticoffset64=1 actual_ring_llvm=matched\n");
    }
    void unused_negative(char const* path, error_code expected)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-memory64-dual-unused-invalid", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 2uz,
            "actual empty start followed by genuinely unused modern invalid body");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual optional native target");
        dual::emission_policy policy{}; policy.target = machine.get(); policy.emit_integer_scalar_memory = true;
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
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_MEMORY64 unused_invalid_diagnostic=", static_cast<unsigned>(expected), " accepted=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 5, "positive and three official parsed modern unused-invalid Wasm files");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual LLVM native target initialization");
    install_unexpected_traps();
    positive(argv[1]);
    unused_negative(argv[2], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[3], error_code::store_value_type_mismatch);
    unused_negative(argv[4], error_code::illegal_memarg_alignment);
}

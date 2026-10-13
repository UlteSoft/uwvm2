// Actual parser/initializer + whole-module admission. Only invalid fixtures
// with uncalled bodies are accepted as input; no pure-validation prepass.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#if __has_include(<uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#define UWVM2TEST_ADMISSION_INT_LAZY 1
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#if __has_include(<uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#define UWVM2TEST_ADMISSION_LLVM_LAZY 1
#endif
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    using validation_error = ::uwvm2::validation::error::code_validation_error_impl;
    unsigned failures{}, observations{};
    auto all_features()
    {
        auto out{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(out)};
        policy.disable_gc = false; policy.disable_function_references = false;
        policy.disable_exceptions = false; policy.disable_tail_call = false;
        policy.disable_memory64 = false;
        return out;
    }
    template<typename Operation>
    void must_reject(char const* file, char const* mode, runtime_module_t const& module,
        validation_error& error, Operation operation)
    {
        bool typed_refusal{};
        try { operation(); }
        catch(::fast_io::error const& caught)
        {
            if(caught.domain != ::fast_io::parse_domain_value || caught.code !=
               static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            // Genuine authoritative instruction diagnostic must refer to the
            // unused penultimate body, never the empty final exported _start.
            auto const count{module.local_defined_function_vec_storage.size()};
            if(count < 2uz) { ::fast_io::fast_terminate(); }
            auto const& unused{module.local_defined_function_vec_storage.index_unchecked(count - 2uz)};
            auto const code{unused.wasm_code_ptr};
            if(code == nullptr) { ::fast_io::fast_terminate(); }
            // [actual immutable unused expression begin ...] | code_end
            // [safe diagnostic only: integer interval test  ] | one-past excluded
            auto const begin{reinterpret_cast<::std::uintptr_t>(code->body.expr_begin)};
            auto const end{reinterpret_cast<::std::uintptr_t>(code->body.code_end)};
            auto const position{reinterpret_cast<::std::uintptr_t>(error.err_curr)};
            typed_refusal = error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok &&
                begin != 0u && end > begin && position >= begin && position < end;
        }
        ++observations; failures += !typed_refusal;
        ::fast_io::print(::fast_io::out(), "LAZY_ADMISSION file=", ::fast_io::mnp::os_c_str(file),
            " mode=", ::fast_io::mnp::os_c_str(mode), " unused_instruction_refusal=", ::fast_io::mnp::dec(typed_refusal),
            " guest_execution=0 native_codegen=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    if(argc != 7) { ::fast_io::fast_terminate(); } // Six exact invalid WAT binaries.
    for(int index{1}; index != argc; ++index)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[index]), ::fast_io::open_mode::in};
        if(input.size() < 8uz || input.size() > 65536uz) { ::fast_io::fast_terminate(); }
        byte_vec source(input.size());
        // [RAII actual file image / exactly sized owned source] equal extents
        // [safe: prior 8..65536 byte bound; no pointer advance]
        ::fast_io::freestanding::my_memcpy(source.data(), input.data(), input.size());
        auto features{all_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"unused-admission", {}, features)};
        if(prepared.mod == nullptr) { ::fast_io::fast_terminate(); }
        validation_error int_error{};
        must_reject(argv[index], "int-full", *prepared.mod, int_error, [&]
        {
            optable::compile_option options{};
            auto compiled{compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(*prepared.mod, options, int_error, &features)};
            (void)compiled;
        });
#if defined(UWVM2TEST_ADMISSION_INT_LAZY)
        validation_error int_lazy_error{};
        must_reject(argv[index], "int-lazy-admission", *prepared.mod, int_lazy_error, [&]
        {
            namespace lazy = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator;
            optable::compile_option options{};
            auto compiled{lazy::initialize_lazy_module_storage<k_test_byref_opt>(*prepared.mod, options, int_lazy_error, {}, &features)};
            (void)compiled;
        });
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        namespace jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        validation_error llvm_error{};
        must_reject(argv[index], "llvm-full", *prepared.mod, llvm_error, [&]
        {
            jit::compile_option options{}; options.validator_feature_parameter = &features;
            auto compiled{jit::compile_all_from_uwvm(*prepared.mod, options, llvm_error, 0uz)};
            (void)compiled;
        });
#if defined(UWVM2TEST_ADMISSION_LLVM_LAZY)
        validation_error llvm_lazy_error{};
        must_reject(argv[index], "llvm-lazy-admission", *prepared.mod, llvm_lazy_error, [&]
        {
            namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
            jit::compile_option options{}; options.validator_feature_parameter = &features;
            auto compiled{lazy::initialize_lazy_module_storage(*prepared.mod, options, llvm_lazy_error)};
            (void)compiled;
        });
#endif
#endif
    }
    ::fast_io::print(::fast_io::out(), "LAZY_ADMISSION observations=", ::fast_io::mnp::dec(observations),
        " failures=", ::fast_io::mnp::dec(failures), " no_guest_execution=1\n");
    return failures != 0u;
}

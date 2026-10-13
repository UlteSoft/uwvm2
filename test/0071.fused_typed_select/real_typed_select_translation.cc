// Actual parser/initializer -> sole selected backend's fused validator/translator.
// No pure validation pass. Native execution is separately tested by actual CLI.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#endif
namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    void require(bool valid, char const* reason)
    { if(!valid) { ::fast_io::print(::fast_io::err(), "actual typed select family: ", ::fast_io::mnp::os_c_str(reason), "\n"); ::fast_io::fast_terminate(); } }
    byte_vec source_file(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path),::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz,"bounded actual complete source");
        byte_vec source(input.size());
        // [RAII retained whole source][equally sized owned bytes] end
        // Checked full extents before exact copy; no cursor is advanced.
        ::fast_io::freestanding::my_memcpy(source.data(),input.data(),input.size()); return source;
    }
    auto features()
    {
        auto result{make_wasm1p1_feature_parameter()};
        auto& flags{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        flags.disable_simd = false; flags.disable_gc = false; flags.disable_function_references = false;
        flags.disable_exceptions = false; return result;
    }
    void check(char const* path, bool invalid, bool gc_off = false)
    {
        auto source{source_file(path)}; auto selected_features{features()};
        if(gc_off)
        {
            auto& flags{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(selected_features)};
            flags.disable_gc = true; flags.disable_function_references = true;
        }
        auto prepared{prepare_runtime_from_wasm(source,u8"same-first-typed-select",{},selected_features)};
        require(bool(prepared.mod),"actual initialized module");
        ::uwvm2::validation::error::code_validation_error_impl error{};
        bool admitted{};
        try
        {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            namespace jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
            jit::compile_option options{}; options.validator_feature_parameter = &selected_features;
            options.verify_llvm_jit_ir = true;
# if defined(UWVM2TEST_SELECT_UNWIND)
            options.emit_call_stack_frames = false; options.emit_unwind_call_stack_frames = true;
# else
            options.emit_call_stack_frames = true; options.emit_unwind_call_stack_frames = false;
# endif
            auto compiled{jit::compile_all_from_uwvm(*prepared.mod,options,error,0uz)};
            require(!invalid && compiled.llvm_jit_module.emitted && compiled.llvm_jit_module.llvm_module,
                "legal modern typed select must produce actual complete LLVM IR");
            require(!::llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,::std::addressof(::llvm::errs())),"real module verifier");
            ::std::size_t selects{};
            for(auto const& function : *compiled.llvm_jit_module.llvm_module)
            { for(auto const& block : function) { for(auto const& instruction : block)
              { if(::llvm::isa<::llvm::SelectInst>(instruction)) { ++selects; } } } }
            ::fast_io::print(::fast_io::out(),"TYPED_SELECT_IR select_instructions=",selects,
                " gc_off=",gc_off," optimized_shape_not_fixed=1\n");
#else
# if defined(UWVM2TEST_SELECT_CACHED_RING)
            constexpr auto option{make_tailcall_fully_split_opt<3uz,3uz,8uz,8uz>()};
            static_assert(option.is_tail_call && option.i32_stack_top_begin_pos != option.i32_stack_top_end_pos &&
                option.i64_stack_top_begin_pos != option.i64_stack_top_end_pos);
# else
            constexpr auto option{k_test_byref_opt};
# endif
            optable::compile_option options{};
            auto compiled{compiler::compile_all_from_uwvm_single_func<option>(*prepared.mod,options,error,&selected_features)};
            require(!invalid && compiled.local_funcs.size() == (gc_off ? 1uz : 10uz),"all10 actual typed bodies/ring fixups complete");
            auto const result{interpreter_runner<option>::run(compiled.local_funcs.index_unchecked(gc_off ? 0uz : 9uz),
                prepared.mod->local_defined_function_vec_storage.index_unchecked(gc_off ? 0uz : 9uz),{},nullptr,nullptr)};
            require(result.results.empty(),"genuine called start checks numeric/SIMD/GC/exception typed selection");
#endif
            admitted = true;
        }
        catch(::fast_io::error const&) { require(invalid,"positive never silently catches emission/validation failure"); }
        require(admitted != invalid && (error.err_code == error_code::ok) != invalid,"unused invalid body rejected during whole admission");
        if(invalid) { require(error.err_code == error_code::select_cond_type_not_i32 ||
            error.err_code == error_code::select_type_mismatch || error.err_code == error_code::operand_stack_underflow,
            "precise select code error, no CLI/I/O/unsupported-native proxy"); }
    }
}
int main(int argc,char** argv)
{
    require(argc == 9,"exact positive plus6 modern unused-negative paths and actual GC-off exn positive");
    for(int i{1}; i != 8; ++i) { check(argv[i],i != 1); }
    check(argv[8],false,true);
    ::fast_io::print(::fast_io::out(),"TYPED_SELECT_REAL_WALK modern_unused=6 declared_numeric_SIMD_reference_types=1 prior_inline_select_preserved=1 full_tiered_qualified=0\n");
}

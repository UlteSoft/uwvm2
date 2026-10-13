#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# include <llvm/IR/Verifier.h>
# include <llvm/IR/Instructions.h>
# include <llvm/Support/raw_ostream.h>
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace wasm3 = uwvm2::validation::standard::wasm3;
    using error_t = uwvm2::validation::error::code_validation_error_impl;
    using error_code = uwvm2::validation::error::code_validation_error_code;
    void bytes(byte_vec& out, std::initializer_list<unsigned char> values)
    { for(auto v: values) { append_u8(out, v); } }
    auto const& code_section()
    {
        auto const& parsed{uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        return []<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections, uwvm2::utils::container::tuple<Fs...>)->auto const&
        { return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections); }
            (parsed.sections, uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }
    int validate_pure(auto const& features, bool accepted, unsigned expected_opcode)
    {
        auto const& parsed{uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        auto const& body{code_section().codes.index_unchecked(0).body};
        error_t error{};
        try { wasm3::validate_code_with_runtime_policy(parsed, 0,
            reinterpret_cast<std::byte const*>(body.expr_begin), reinterpret_cast<std::byte const*>(body.code_end), error, features); }
        catch(fast_io::error const&) {}
        UWVM2TEST_REQUIRE((error.err_code == error_code::ok) == accepted);
        if(error.err_code == error_code::wasm1p1_feature_required)
        {
            UWVM2TEST_REQUIRE(error.err_selectable.wasm1p1_feature_required.feature == uwvm2::parser::wasm::base::wasm1p1_feature_kind::tail_call);
            UWVM2TEST_REQUIRE(error.err_selectable.wasm1p1_feature_required.value == expected_opcode);
        }
        return 0;
    }
    // Core 3.0 tail-call validation differs from ordinary call followed by return:
    // result compatibility is checked even in unreachable code and no results are
    // left on the current block's operand stack. Params may differ from caller.
    int validation_cases()
    {
        unsigned checks{};
        for(bool indirect: {false, true}) for(bool dead: {false, true})
        for(unsigned variation{}; variation != 12; ++variation) for(bool disabled: {false, true})
        {
            auto features{make_wasm1p1_feature_parameter()};
            auto& policy{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
            policy.cli_mode = uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;
            policy.disable_tail_call = disabled;
            module_builder builder{};
            builder.has_table = true; builder.table_min = 1;
            if(variation == 10 && indirect) { builder.table_elem_type = 0x6f; }
            func_type caller{{}, {0x7f, 0x7e}};
            func_type callee{{0x7e}, {0x7f, 0x7e}};
            if(variation == 1) { callee.results = {0x7f}; }
            if(variation == 2) { callee.results = {0x7e, 0x7f}; }
            func_body body{};
            if(dead) { bytes(body.code, {0x00}); }
            if(variation == 11) { bytes(body.code, {0x02, 0x40}); }
            if(variation != 3) { bytes(body.code, {static_cast<unsigned char>(variation == 4 ? 0x41 : 0x42), 9}); }
            if(indirect) { bytes(body.code, {static_cast<unsigned char>(variation == 5 ? 0x42 : 0x41), 0}); }
            append_u8(body.code, indirect ? 0x13 : 0x12);
            if(variation == 6) { bytes(body.code, {0xff, 0xff, 0xff, 0xff, 0x0f}); }
            else if(variation == 7) { bytes(body.code, {0x81, 0x80, 0x80, 0x80, 0x00}); }
            else if(variation == 8) { bytes(body.code, {0x81, 0x80, 0x80, 0x80, 0x10}); }
            else { append_u8(body.code, 1); }
            if(indirect)
            {
                if(variation == 9) { bytes(body.code, {1}); }
                else { bytes(body.code, {0x80, 0}); } // padded tableidx zero is legal
            }
            if(variation == 11) { bytes(body.code, {0x0b, 0x00}); }
            bytes(body.code, {0x0b});
            builder.add_func(std::move(caller), std::move(body));
            func_body target{}; bytes(target.code, {0x00, 0x0b});
            builder.add_func(std::move(callee), std::move(target));
            auto wasm{builder.build()};
            auto prepared{prepare_runtime_from_wasm(wasm, u8"tail-validation", {}, features)};
            bool valid = variation == 0 || variation == 7 || variation == 11 ||
                (variation == 3 && dead) || (!indirect && (variation == 5 || variation == 9 || variation == 10));
            UWVM2TEST_REQUIRE(validate_pure(features, valid && !disabled, indirect ? 0x13 : 0x12)==0);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            error_t integrated_error{};
            namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
            jit::compile_option options{}; options.validator_feature_parameter=&features;
            try
            {
                auto compiled{jit::compile_all_from_uwvm(*prepared.mod, options, integrated_error, 0)};
                static_cast<void>(compiled);
            }
            catch(fast_io::error const&) {}
            UWVM2TEST_REQUIRE((integrated_error.err_code == error_code::ok) == (valid && !disabled));
#else
            error_t integrated_error{};
            optable::compile_option options{};
            try
            {
                auto compiled{compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(*prepared.mod, options, integrated_error, &features)};
                static_cast<void>(compiled);
            }
            catch(fast_io::error const&) {}
            UWVM2TEST_REQUIRE((integrated_error.err_code == error_code::ok) == (valid && !disabled));
#endif
            ++checks;
        }
        std::printf("PASS tail-call pure + integrated validator: %u direct/indirect, tuple, polymorphic, padded-index and gate cases\n", checks);
        return 0;
    }
    byte_vec self_module()
    {
        module_builder builder{};
        func_body body{}; body.locals.push_back({1, 0x7f});
        // Assert the declared local is zero on every activation, then dirty it.
        bytes(body.code, {0x20,5, 0x45, 0x04,0x40, 0x05,0x00,0x0b, 0x41,23,0x21,5});
        bytes(body.code, {0x20,0,0x45,0x04,0x40});
        bytes(body.code, {0x20,1,0x20,2,0x20,3,0x20,4,0x0f,0x0b});
        // An extra operand below the parameters must be discarded. Swap params
        // 1 and 2 to expose serial-copy aliasing; preserve mixed ring registers.
        bytes(body.code, {0x42,37, 0x20,0,0x41,1,0x6b, 0x20,2,0x20,1, 0x20,3,0x42,7,0x7c, 0x20,4, 0x12,0,0x0b});
        builder.add_func({{0x7f,0x7f,0x7f,0x7e,0x7c},{0x7f,0x7f,0x7e,0x7c}}, std::move(body));
        return builder.build();
    }
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    template<optable::uwvm_interpreter_translate_option_t Option>
#endif
    int execute_self()
    {
        auto features{make_wasm1p1_feature_parameter()};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_tail_call = false;
        auto wasm{self_module()};
        auto prepared{prepare_runtime_from_wasm(wasm,u8"tail-self",{},features)};
        UWVM2TEST_REQUIRE(validate_pure(features, true, 0x12)==0);
        error_t error{};
        byte_vec arguments(28), results(24);
        std::uint32_t n{1000001}, a{7}, b{9}; std::uint64_t sum{13}; double value{0.25};
        std::memcpy(arguments.data(), &n, 4); std::memcpy(arguments.data()+4, &a, 4);
        std::memcpy(arguments.data()+8, &b, 4); std::memcpy(arguments.data()+12, &sum, 8);
        std::memcpy(arguments.data()+20, &value, 8);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        jit::compile_option options{}; options.validator_feature_parameter=&features; options.verify_llvm_jit_ir=true;
        auto compiled{jit::compile_all_from_uwvm(*prepared.mod,options,error,0)};
        UWVM2TEST_REQUIRE(error.err_code==error_code::ok && compiled.llvm_jit_module.emitted);
        auto& module{*compiled.llvm_jit_module.llvm_module};
        UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
        unsigned backedges{};
        for(auto& f:module) for(auto& block:f) for(auto& inst:block)
        {
            if(auto* branch=llvm::dyn_cast<llvm::BranchInst>(&inst))
            { for(unsigned i{}; i!=branch->getNumSuccessors(); ++i) if(branch->getSuccessor(i)->getName()=="body") { ++backedges; } }
        }
        UWVM2TEST_REQUIRE(backedges >= 2);
        uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,0,results.data(),results.size(),arguments.data(),arguments.size());
#else
        optable::compile_option options{};
        auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features)};
        UWVM2TEST_REQUIRE(error.err_code==error_code::ok);
        auto result{interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(0),arguments,nullptr,nullptr)};
        results = std::move(result.results);
#endif
        UWVM2TEST_REQUIRE(results.size()==24);
        std::memcpy(&a,results.data(),4); std::memcpy(&b,results.data()+4,4);
        std::memcpy(&sum,results.data()+8,8); std::memcpy(&value,results.data()+16,8);
        if(a!=9 || b!=7 || sum!=7000020 || value!=0.25) { std::fprintf(stderr, "tail results: a=%u b=%u sum=%llu value=%g\n", a, b, static_cast<unsigned long long>(sum), value); }
        UWVM2TEST_REQUIRE(a==9 && b==7 && sum==7000020 && value==0.25);
        std::puts("PASS return_call self: 1000001 tail activations, tuple results, mixed params, local reinitialization, operand discard");
        return 0;
    }
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    int execute_mutual()
    {
        auto features{make_wasm1p1_feature_parameter()};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_tail_call=false;
        module_builder builder{};
        for(unsigned target: {1u, 0u})
        {
            func_body body{}; body.locals.push_back({target == 1 ? 1u : 4096u, k_val_i64});
            bytes(body.code, {0x20,3,0x50,0x04,0x40,0x05,0x00,0x0b,0x42,23,0x21,3,
                0x20,0,0x45,0x04,0x40,0x20,1,0x0f,0x0b,
                0x20,0,0x41,1,0x6b,0x20,1,0x42,7,0x7c,0x20,2,0x12});
            append_u8(body.code,target); append_u8(body.code,0x0b);
            builder.add_func({{k_val_i32,k_val_i64,k_val_f64},{k_val_i64}},std::move(body));
        }
        auto wasm{builder.build()};
        auto prepared{prepare_runtime_from_wasm(wasm,u8"tail-mutual",{},features)};
        namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        jit::compile_option options{}; options.validator_feature_parameter=&features; options.verify_llvm_jit_ir=true;
        error_t error{};
        auto compiled{jit::compile_all_from_uwvm(*prepared.mod,options,error,0)};
        UWVM2TEST_REQUIRE(error.err_code==error_code::ok && compiled.llvm_jit_module.emitted);
        auto& module{*compiled.llvm_jit_module.llvm_module};
        UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
        unsigned transfers{};
        for(auto& f:module) for(auto& block:f) for(auto& inst:block)
            if(auto call=llvm::dyn_cast<llvm::CallInst>(&inst);call && call->isMustTailCall()) {++transfers;}
        UWVM2TEST_REQUIRE(transfers==2);
        if(auto file=std::getenv("UWVM_TAIL_IR_FILE"))
        {
            std::error_code ec; llvm::raw_fd_ostream stream(file,ec);
            UWVM2TEST_REQUIRE(!ec); module.print(stream,nullptr);
        }
        byte_vec args(20), result(8);
        std::uint32_t n=1000001; std::uint64_t sum=13; double v=0.25;
        std::memcpy(args.data(),&n,4);std::memcpy(args.data()+4,&sum,8);std::memcpy(args.data()+12,&v,8);
        uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,0,result.data(),result.size(),args.data(),args.size());
        std::memcpy(&sum,result.data(),8);
        UWVM2TEST_REQUIRE(sum==7000020);
        std::puts("PASS JIT mutual return_call: 1000001 transfers, 2 verified native musttail edges, unequal local-frame sizes");
        return 0;
    }
#endif
}
int main()
{
    UWVM2TEST_REQUIRE(validation_cases()==0);
#if defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    UWVM2TEST_REQUIRE(execute_self()==0);
    UWVM2TEST_REQUIRE(execute_mutual()==0);
#else
    install_unexpected_traps();
    UWVM2TEST_REQUIRE(execute_self<k_test_byref_opt>()==0);
#if !defined(UWVM2TEST_TAIL_BYREF_ONLY)
    UWVM2TEST_REQUIRE(execute_self<k_test_tail_min_opt>()==0);
    UWVM2TEST_REQUIRE(execute_self<make_tailcall_scalar4_merged_opt<2>()>()==0);
#endif
#endif
}

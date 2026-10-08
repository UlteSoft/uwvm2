#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <uwvm2/runtime/wasm_threads/impl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# include <llvm/IR/Verifier.h>
# include <llvm/Support/raw_ostream.h>
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace wasm3=uwvm2::validation::standard::wasm3;
    namespace waiting=uwvm2::runtime::wasm_threads;
    using error_t=uwvm2::validation::error::code_validation_error_impl;
    using error_code=uwvm2::validation::error::code_validation_error_code;
    byte_vec make_module(unsigned opcode,bool indexed,unsigned scenario)
    {
        module_builder module{};
        module.has_memory=module.memory_has_max=true;
        module.memory_shared=scenario!=10;
        module.memory_min=module.memory_max=1;
        if(indexed){module.extra_memories.push_back({1,1,true,true});}
        func_body body{};
        func_type type{{k_val_i32},{k_val_i32}};
        bool const unreachable=scenario==1;
        if(unreachable){append_u8(body.code,0x00);}
        else
        {
            // Preserve mixed older values through the memory-only blocking bridge.
            append_u8(body.code,0x41);append_i32_leb(body.code,37);
            append_u8(body.code,0x42);append_i64_leb(body.code,5);
            if(scenario==4){append_u8(body.code,0x42);append_i64_leb(body.code,0);}
            else{append_u8(body.code,0x20);append_u32_leb(body.code,0);}
            bool const wide=(opcode==2)^(scenario==3);
            append_u8(body.code,wide?0x42:0x41);append_u8(body.code,0);
            if(opcode!=0 && scenario!=8)
            {append_u8(body.code,scenario==2?0x41:0x42);append_u8(body.code,0);}
        }
        append_u8(body.code,0xfe);append_u32_leb(body.code,opcode);
        append_u32_leb(body.code,(opcode==2?3:2)+(scenario==5?1:scenario==6?-1:0)+(indexed?64:0));
        if(indexed){append_u32_leb(body.code,scenario==9?2:1);}
        append_u32_leb(body.code,8);
        if(!unreachable)
        {for(auto b:{0xac,0x7c,0xa7,0x6a}){append_u8(body.code,b);}}
        append_u8(body.code,0x0b);
        module.add_func(std::move(type),std::move(body));return module.build();
    }
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    void UWVM2TEST_WASM_ABI align_trap() noexcept {::_exit(41);}
    void UWVM2TEST_WASM_ABI bounds_trap(uwvm2::object::memory::error::memory_error_t const&) noexcept {::_exit(42);}
    void UWVM2TEST_WASM_ABI wait_trap(unsigned status) noexcept
    {::_exit(status==static_cast<unsigned>(waiting::wait_status::not_shared)?43:98);}
#endif
    template<typename Run>
    int check_trap(Run&& run,std::uint32_t address,unsigned kind)
    {
        int output[2]{};UWVM2TEST_REQUIRE(::pipe(output)==0);
        auto child=::fork();UWVM2TEST_REQUIRE(child>=0);
        if(child==0)
        {
            ::close(output[0]);(void)::dup2(output[1],STDERR_FILENO);::close(output[1]);::alarm(15);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
#endif
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
            optable::trap_unaligned_atomic_func=align_trap;
            optable::trap_memory_out_of_bounds_func=bounds_trap;
            optable::trap_atomic_wait_func=wait_trap;
#endif
            (void)run(address);::_exit(99);
        }
        ::close(output[1]);std::string diagnostic;char buffer[1024];
        for(;;){auto n=::read(output[0],buffer,sizeof(buffer));if(n<=0){break;}diagnostic.append(buffer,static_cast<std::size_t>(n));}
        ::close(output[0]);int status{};UWVM2TEST_REQUIRE(::waitpid(child,&status,0)==child);
#if defined(UWVM2TEST_STRICT_NO_INTERPRETER)
        auto expected=kind==41?"unaligned atomic memory access":kind==42?"memory access out of bounds":"atomic wait on non-shared memory";
        if(!WIFSIGNALED(status)||diagnostic.find(expected)==std::string::npos||diagnostic.find("func_idx=")==std::string::npos)
        {std::fprintf(stderr,"bad wait trap: status=%d diagnostic=%s\n",status,diagnostic.c_str());std::abort();}
#else
        UWVM2TEST_REQUIRE(WIFEXITED(status)&&WEXITSTATUS(status)==int(kind));
#endif
        return 0;
    }
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    template<optable::uwvm_interpreter_translate_option_t Option>
#endif
    int check()
    {
        auto features=make_wasm1p1_feature_parameter();
        auto& selected=uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features);
        unsigned accepted{},rejected{};
        for(unsigned opcode{};opcode!=3;++opcode)for(bool indexed:{false,true})for(unsigned scenario{};scenario!=11;++scenario)
        {
            if((opcode==0&&(scenario==2||scenario==8))||(!indexed&&scenario==9)||(indexed&&scenario==10)){continue;}
            selected.disable_threads=selected.disable_multi_memory=false;
            auto wasm=make_module(opcode,indexed,scenario);
            auto prepared=prepare_runtime_from_wasm(wasm,u8"wait-notify",{},features);
            if(scenario==7){selected.disable_threads=true;}
            bool const expected=scenario==0||scenario==1||scenario==10;
            auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;
            auto const& codes=[]<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,uwvm2::utils::container::tuple<Fs...>)->auto const&
            {return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);}
                (parsed.sections,uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
            auto const& body=codes.codes.index_unchecked(0).body;
            error_t standalone{};
            try{wasm3::validate_code_with_runtime_policy(parsed,0,reinterpret_cast<std::byte const*>(body.expr_begin),reinterpret_cast<std::byte const*>(body.code_end),standalone,features);}
            catch(fast_io::error const&){}
            UWVM2TEST_REQUIRE((standalone.err_code==error_code::ok)==expected);
            error_t integrated{};
            try
            {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
                namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
                jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
                bool const unwind=uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack==uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_t::unwind;
                options.emit_call_stack_frames=!unwind;options.emit_unwind_call_stack_frames=unwind;
                auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,integrated,0);
                UWVM2TEST_REQUIRE(expected&&compiled.llvm_jit_module.emitted);
                auto& module=*compiled.llvm_jit_module.llvm_module;
                UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
                if(auto dir=std::getenv("UWVM_WAIT_IR_DIR");dir!=nullptr&&scenario==0)
                {
                    std::error_code error;
                    auto path=std::string(dir)+"/wait-"+std::to_string(opcode)+(indexed?"-indexed-":"-legacy-")+(unwind?"unwind.ll":"instruction.ll");
                    llvm::raw_fd_ostream output(path,error);UWVM2TEST_REQUIRE(!error);module.print(output,nullptr);
                }
#else
                optable::compile_option options{};
                auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,integrated,&features);
                UWVM2TEST_REQUIRE(expected);
#endif
                if(scenario!=1)
                {
                    auto run=[&](std::uint32_t address)->std::uint32_t
                    {
                        std::uint32_t result{};
                        auto args=pack_i32(static_cast<std::int32_t>(address));
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
                        uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,0,&result,4,args.data(),4);
#else
                        waiting::wait_domain domain;
                        waiting::execution_scope scope{domain,{}};
                        auto out=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),prepared.mod->local_defined_function_vec_storage.index_unchecked(0),args,nullptr,nullptr);
                        UWVM2TEST_REQUIRE(out.results.size()==4);std::memcpy(&result,out.results.data(),4);
#endif
                        return result;
                    };
                    if(scenario==10&&opcode!=0){UWVM2TEST_REQUIRE(check_trap(run,0,43)==0);}
                    else
                    {
                        UWVM2TEST_REQUIRE(run(0)==(opcode==0?42:44));
                        UWVM2TEST_REQUIRE(run(65528-(opcode==2?8:4))==(opcode==0?42:44));
                        UWVM2TEST_REQUIRE(check_trap(run,1,41)==0);UWVM2TEST_REQUIRE(check_trap(run,65528,42)==0);UWVM2TEST_REQUIRE(check_trap(run,0xfffffff8u,42)==0);
                    }
                }
            }
            catch(fast_io::error const&){}
            UWVM2TEST_REQUIRE((integrated.err_code==error_code::ok)==expected);
            if(expected){++accepted;}else{++rejected;}
        }
        std::printf("wait/notify: %u accepted %u rejected; mixed types, unreachable, indexed memory, zero count, u33 bounds, traps PASS\n",accepted,rejected);
        return 0;
    }
}
int main(int argc,char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    mode::global_runtime_llvm_jit_call_stack=argc>1&&std::strcmp(argv[1],"unwind")==0?mode::runtime_llvm_jit_call_stack_t::unwind:mode::runtime_llvm_jit_call_stack_t::instruction;
#endif
#if defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    return check();
#else
    (void)argc;(void)argv;install_unexpected_traps();
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call=false};
#if !defined(UWVM2TEST_UNCACHED_ONLY)
    constexpr optable::uwvm_interpreter_translate_option_t merged{
        .is_tail_call=true,.i32_stack_top_begin_pos=3uz,.i32_stack_top_end_pos=5uz,
        .i64_stack_top_begin_pos=3uz,.i64_stack_top_end_pos=5uz,
        .f32_stack_top_begin_pos=5uz,.f32_stack_top_end_pos=7uz,
        .f64_stack_top_begin_pos=5uz,.f64_stack_top_end_pos=7uz};
    constexpr auto separate=[=]{auto option=merged;option.i64_stack_top_begin_pos=7uz;option.i64_stack_top_end_pos=9uz;return option;}();
    UWVM2TEST_REQUIRE(check<merged>()==0);UWVM2TEST_REQUIRE(check<separate>()==0);
#endif
    return check<uncached>();
#endif
}

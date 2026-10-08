#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#if defined(UWVM2TEST_FENCE_LAZY)
# include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#endif
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
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    void UWVM2TEST_WASM_ABI alignment_trap() noexcept {::_exit(41);}
    void UWVM2TEST_WASM_ABI bounds_trap(uwvm2::object::memory::error::memory_error_t const&) noexcept {::_exit(42);}
    void bounds_signal(uwvm2::object::memory::error::mmap_memory_error_t const&) noexcept {::_exit(42);}
#endif
    template<typename Run>
    int check_trap(Run&& run,std::uint32_t address,bool alignment)
    {
        int output[2]{}; UWVM2TEST_REQUIRE(::pipe(output)==0);
        auto const child{::fork()}; UWVM2TEST_REQUIRE(child>=0);
        if(child==0)
        {
            ::close(output[0]); (void)::dup2(output[1],STDERR_FILENO); ::close(output[1]); ::alarm(15);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            // The runtime owns a duplicate of stderr; rebind that diagnostic
            // descriptor too, so a real trap is distinguished from a raw signal.
            uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
#endif
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
            optable::trap_unaligned_atomic_func=alignment_trap;
            optable::trap_memory_out_of_bounds_func=bounds_trap;
            uwvm2::object::memory::signal::set_mmap_memory_out_of_bounds_handler(bounds_signal);
#endif
            (void)run(address);
            ::_exit(99);
        }
        ::close(output[1]);
        std::string diagnostic{}; char buffer[1024]{};
        for(;;) {auto n=::read(output[0],buffer,sizeof(buffer));if(n<=0) {break;} diagnostic.append(buffer,static_cast<std::size_t>(n));}
        ::close(output[0]); int status{}; UWVM2TEST_REQUIRE(::waitpid(child,&status,0)==child);
#if defined(UWVM2TEST_STRICT_NO_INTERPRETER)
        auto const expected{alignment ? "unaligned atomic memory access" : "memory access out of bounds"};
        if(!WIFSIGNALED(status) || diagnostic.find(expected)==std::string::npos || diagnostic.find("func_idx=")==std::string::npos)
        {std::fprintf(stderr,"trap failed address=%u status=%d diagnostic=%s\n",address,status,diagnostic.c_str());return 1;}
#else
        UWVM2TEST_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status)==(alignment ? 41 : 42));
#endif
        return 0;
    }
    byte_vec make_module(unsigned opcode,bool indexed,int bad_alignment,bool bad_address,bool unreachable)
    {
        auto const descriptor{wasm3::describe_atomic_instruction(opcode)};
        module_builder module{};
        module.has_memory=true; module.memory_min=module.memory_max=1; module.memory_has_max=true;
        if(indexed) {module.extra_memories.push_back({1,1,true});}
        func_type type{{bad_address ? k_val_f32 : k_val_i32},{descriptor.result_i64 ? k_val_i64 : k_val_i32}};
        func_body body{};
        if(unreachable) {append_u8(body.code,0);}
        for(unsigned copy{};copy!=2;++copy)
        {
            append_u8(body.code,0x20); append_u8(body.code,0);
            append_u8(body.code,0xfe); append_u32_leb(body.code,opcode);
            append_u32_leb(body.code,static_cast<unsigned>(static_cast<int>(descriptor.natural_alignment)+bad_alignment)+(indexed ? 64 : 0));
            if(indexed) {append_u32_leb(body.code,1);}
            append_u32_leb(body.code,8); // Alignment must check address + offset.
        }
        append_u8(body.code,descriptor.result_i64 ? 0x7c : 0x6a); append_u8(body.code,0x0b);
        module.add_func(std::move(type),std::move(body));
        return module.build();
    }
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    template <optable::uwvm_interpreter_translate_option_t Option>
#endif
    int check()
    {
        auto features{make_wasm1p1_feature_parameter()};
        auto& selected{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        unsigned accepted{},rejected{};
        for(unsigned opcode=0x10;opcode<=0x16;++opcode) for(bool indexed:{false,true})
        for(unsigned scenario{};scenario!=6;++scenario)
        {
            selected.disable_threads=false; selected.disable_multi_memory=false;
            auto const descriptor{wasm3::describe_atomic_instruction(opcode)};
            int const alignment_delta{scenario==1 ? 1 : scenario==2 ? -1 : 0};
            if(alignment_delta<0 && descriptor.natural_alignment==0) {continue;}
            bool const bad_address{scenario==3}, unreachable{scenario==4};
            bool const expected{scenario==0 || scenario==4};
            auto wasm{make_module(opcode,indexed,alignment_delta,bad_address,unreachable)};
            auto prepared{prepare_runtime_from_wasm(wasm,u8"atomic-loads",{},features)};
            if(scenario==5) {selected.disable_threads=true;}
            auto const& parsed{uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
            auto const& codes=[]<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,uwvm2::utils::container::tuple<Fs...>)->auto const&
            {return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);}
                (parsed.sections,uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
            auto const& body{codes.codes.index_unchecked(0).body};
            error_t standalone{};
            try {wasm3::validate_code_with_runtime_policy(parsed,0,reinterpret_cast<std::byte const*>(body.expr_begin),
                reinterpret_cast<std::byte const*>(body.code_end),standalone,features);}
            catch(fast_io::error const&) {}
            UWVM2TEST_REQUIRE((standalone.err_code==error_code::ok)==expected);
            error_t integrated{};
            try
            {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
                namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
                jit::compile_option options{}; options.validator_feature_parameter=&features; options.verify_llvm_jit_ir=true;
                bool const unwind{uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack ==
                    uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_t::unwind};
                options.emit_call_stack_frames=!unwind; options.emit_unwind_call_stack_frames=unwind;
                auto compiled{jit::compile_all_from_uwvm(*prepared.mod,options,integrated,0)};
                UWVM2TEST_REQUIRE(expected && compiled.llvm_jit_module.emitted);
                auto& module{*compiled.llvm_jit_module.llvm_module};
                UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
                if(auto const directory{std::getenv("UWVM_ATOMIC_IR_DIR")}; directory!=nullptr && scenario==0)
                {
                    std::error_code error{};
                    std::string const path{std::string(directory)+"/atomic-"+std::to_string(opcode)+(indexed ? "-indexed-" : "-legacy-")+
                        (unwind ? "unwind.ll" : "instruction.ll")};
                    llvm::raw_fd_ostream output(path,error); UWVM2TEST_REQUIRE(!error);
                    module.print(output,nullptr); output.close(); UWVM2TEST_REQUIRE(!output.has_error());
                }
                unsigned atomics{};
                for(auto& f:module) for(auto& block:f) for(auto& inst:block)
                {if(auto* load=llvm::dyn_cast<llvm::LoadInst>(&inst);load!=nullptr && load->isAtomic())
                    {++atomics; UWVM2TEST_REQUIRE(load->getOrdering()==llvm::AtomicOrdering::SequentiallyConsistent);
                     UWVM2TEST_REQUIRE(load->getAlign().value()==(1u<<descriptor.natural_alignment));}}
                UWVM2TEST_REQUIRE(atomics==(unreachable ? 0u : 2u));
#else
                optable::compile_option options{};
                auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,integrated,&features)};
                UWVM2TEST_REQUIRE(expected);
#endif
                if(!unreachable)
                {
                    auto const& memory{prepared.mod->local_defined_memory_vec_storage.index_unchecked(indexed ? 1 : 0).memory};
                    if(indexed) {std::memset(prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory.memory_begin,0x11,65536);}
                    for(unsigned i{};i!=65536;++i) {memory.memory_begin[i]=static_cast<std::byte>((i*17+0x85)&255);}
                    auto run = [&](std::uint32_t address) -> std::uint64_t
                    {
                        std::uint64_t actual{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
                        std::byte result[8]{};
                        auto arguments{pack_i32(static_cast<std::int32_t>(address))};
                        uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,0,result,descriptor.result_i64 ? 8 : 4,arguments.data(),4);
                        if(descriptor.result_i64) {std::memcpy(&actual,result,8);}
                        else {std::uint32_t narrow{};std::memcpy(&narrow,result,4);actual=narrow;}
#else
                        auto result{interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
                            prepared.mod->local_defined_function_vec_storage.index_unchecked(0),pack_i32(static_cast<std::int32_t>(address)),nullptr,nullptr)};
                        UWVM2TEST_REQUIRE(result.results.size()==(descriptor.result_i64 ? 8 : 4));
                        if(descriptor.result_i64) {std::memcpy(&actual,result.results.data(),8);}
                        else {std::uint32_t narrow{};std::memcpy(&narrow,result.results.data(),4);actual=narrow;}
#endif
                        return actual;
                    };
                    for(unsigned address:{0u,8u,24u,65536u-8u-(1u<<descriptor.natural_alignment)})
                    {
                        std::uint64_t expect{};
                        for(unsigned byte{};byte!=(1u<<descriptor.natural_alignment);++byte)
                        {expect|=static_cast<std::uint64_t>(std::to_integer<unsigned>(memory.memory_begin[address+8+byte]))<<(byte*8);}
                        expect*=2;
                        if(!descriptor.result_i64) {expect=static_cast<std::uint32_t>(expect);}
                        auto const actual{run(address)};
                        UWVM2TEST_REQUIRE(actual==expect);
                    }
                    if(descriptor.natural_alignment!=0) {UWVM2TEST_REQUIRE(check_trap(run,1,true)==0);}
                    UWVM2TEST_REQUIRE(check_trap(run,65528,false)==0); // First byte after the committed page.
                    UWVM2TEST_REQUIRE(check_trap(run,0xfffffff8u,false)==0); // u33 sum must not wrap to memory[0].
                }
            }
            catch(fast_io::error const&) {}
            UWVM2TEST_REQUIRE((integrated.err_code==error_code::ok)==expected);
            if(expected) {++accepted;} else {++rejected;}
        }
        std::printf("PASS atomic loads: %u accepted/%u rejected, seven widths, indexed memory, high-bit zero extension, page end, feature gate\n",accepted,rejected);
        return 0;
    }
}
int main(int argc,char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    mode::global_runtime_llvm_jit_call_stack = argc>1 && std::strcmp(argv[1],"unwind")==0 ?
        mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::instruction;
#else
    (void)argc; (void)argv;
#endif
#if defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    return check();
#else
    install_unexpected_traps();
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call=false};
    constexpr optable::uwvm_interpreter_translate_option_t merged{
        .is_tail_call=true,.i32_stack_top_begin_pos=3uz,.i32_stack_top_end_pos=5uz,
        .i64_stack_top_begin_pos=3uz,.i64_stack_top_end_pos=5uz,
        .f32_stack_top_begin_pos=5uz,.f32_stack_top_end_pos=7uz,
        .f64_stack_top_begin_pos=5uz,.f64_stack_top_end_pos=7uz};
    constexpr auto separate=[=] {auto option=merged;option.i64_stack_top_begin_pos=7uz;option.i64_stack_top_end_pos=9uz;return option;}();
    UWVM2TEST_REQUIRE(check<uncached>()==0);
    UWVM2TEST_REQUIRE(check<merged>()==0);
    return check<separate>();
#endif
}

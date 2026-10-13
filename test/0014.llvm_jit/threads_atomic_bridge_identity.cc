// Every atomic specialization coexists in one native module. Per-opcode modules
// cannot catch MCJIT symbols that collide and overwrite another specialization.
// Compile with UWVM_USE_MULTITHREAD_ALLOCATOR to exercise host bridge fallbacks;
// also run mmap to compare the directly lowered operations against the same oracle.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/validation/standard/wasm3/atomic_immediate.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <bit>
#include <string>
int main(int argc,char** argv)
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace atom=uwvm2::validation::standard::wasm3;
    namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    bool const unwind=argc>1&&std::strcmp(argv[1],"unwind")==0;
    mode::global_runtime_llvm_jit_call_stack=unwind?mode::runtime_llvm_jit_call_stack_t::unwind:mode::runtime_llvm_jit_call_stack_t::instruction;
    constexpr std::uint64_t initial=0x88776655c4332211ULL,operand=0x8182838485868788ULL;
    module_builder builder{};builder.has_memory=builder.memory_has_max=builder.memory_shared=true;
    builder.memory_min=builder.memory_max=1;
    for(unsigned opcode=0x10;opcode<=0x4e;++opcode)
    {
        auto d=atom::describe_atomic_instruction(opcode);func_body body{};
        append_u8(body.code,0x41);append_u8(body.code,0);
        auto value=[&](std::uint64_t v)
        {
            append_u8(body.code,d.value_i64?0x42:0x41);
            if(d.value_i64){append_i64_leb(body.code,std::bit_cast<std::int64_t>(v));}
            else{append_i32_leb(body.code,std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(v)));}
        };
        if(d.kind==atom::atomic_instruction_kind::compare_exchange){value(initial);}
        if(d.operand_count>1){value(operand);}
        append_u8(body.code,0xfe);append_u32_leb(body.code,opcode);append_u32_leb(body.code,d.natural_alignment);append_u8(body.code,0);
        if(!d.has_result){append_u8(body.code,0x42);append_u8(body.code,0);}
        else if(!d.result_i64){append_u8(body.code,0xad);}
        append_u8(body.code,0x0b);builder.add_func({{}, {k_val_i64}},std::move(body));
    }
    for(unsigned opcode{};opcode!=3;++opcode)
    {
        func_body body{};for(auto b:{0x41,0,opcode==2?0x42:0x41,1}){append_u8(body.code,b);}
        if(opcode){append_u8(body.code,0x42);append_u8(body.code,0x7f);}
        for(auto b:{0xfe,int(opcode),opcode==2?3:2,0,0xad,0x0b}){append_u8(body.code,b);}
        builder.add_func({{}, {k_val_i64}},std::move(body));
    }
    auto features=make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads=false;
    auto wasm=builder.build();auto prepared=prepare_runtime_from_wasm(wasm,u8"mixed-atomic-bridges",{},features);
    jit::compile_option options{};options.validator_feature_parameter=&features;options.emit_call_stack_frames=!unwind;options.emit_unwind_call_stack_frames=unwind;
    uwvm2::validation::error::code_validation_error_impl error{};
    auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);
    UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);
    auto& module=*compiled.llvm_jit_module.llvm_module;UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
    if(auto dir=std::getenv("UWVM_ATOMIC_IR_DIR");dir!=nullptr)
    {
        std::error_code ec;auto path=std::string(dir)+(unwind?"/mixed-unwind.ll":"/mixed-instruction.ll");
        llvm::raw_fd_ostream out(path,ec);UWVM2TEST_REQUIRE(!ec);module.print(out,nullptr);
    }
    auto& memory=prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory;
    auto reset=[&]{for(unsigned i{};i!=8;++i){memory.memory_begin[i]=static_cast<std::byte>(initial>>(8*i));}};
    auto run=[&](unsigned index)
    {std::uint64_t result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,&result,8,nullptr,0);return result;};
    for(unsigned opcode=0x10;opcode<=0x4e;++opcode)
    {
        reset();auto d=atom::describe_atomic_instruction(opcode);unsigned const bytes=1u<<d.natural_alignment;
        auto const mask=bytes==8?UINT64_MAX:(std::uint64_t{1}<<(bytes*8))-1;
        auto const old=initial&mask;auto const value=operand&mask;std::uint64_t next=old;
        switch(d.kind)
        {
            case atom::atomic_instruction_kind::load:break;
            case atom::atomic_instruction_kind::store:case atom::atomic_instruction_kind::exchange:case atom::atomic_instruction_kind::compare_exchange:next=value;break;
            case atom::atomic_instruction_kind::add:next=old+value;break;
            case atom::atomic_instruction_kind::sub:next=old-value;break;
            case atom::atomic_instruction_kind::and_:next=old&value;break;
            case atom::atomic_instruction_kind::or_:next=old|value;break;
            case atom::atomic_instruction_kind::xor_:next=old^value;break;
            default:std::abort();
        }
        auto result=run(opcode-0x10);
        UWVM2TEST_REQUIRE(result==(d.has_result?old:0));
        std::uint64_t actual{};for(unsigned i{};i!=8;++i){actual|=std::uint64_t{std::to_integer<unsigned>(memory.memory_begin[i])}<<(8*i);}
        UWVM2TEST_REQUIRE(actual==((initial&~mask)|(next&mask)));
    }
    reset();UWVM2TEST_REQUIRE(run(63)==0);UWVM2TEST_REQUIRE(run(64)==1);UWVM2TEST_REQUIRE(run(65)==1);
    std::printf("PASS 66 coexisting atomic bridge identities: all widths/families, wait/notify, %s, mmap=%d\n",unwind?"unwind":"instruction",int(std::remove_reference_t<decltype(memory)>::can_mmap));
}

// Actual production copier -> LLVM verifier -> native MCJIT / target llc.
// The local declarations below are component DATA, never Wasm source leases,
// activations, host-address authority or checkpoint restore admission.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <fast_io.h>
#include <array>
#include <bit>
#include <cstdint>
#include <memory>
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace support = ::uwvm2::runtime::compiler::llvm_jit::details;
static void require(bool ok, char const* message)
{
    if(!ok) { ::fast_io::io::perrln("snapshot copy IR: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
class fast_io_ir_stream final : public ::llvm::raw_ostream
{
    ::fast_io::native_io_observer out_;
    ::std::uint64_t position_{};
    void write_impl(char const* bytes, ::std::size_t size) override
    {
        if(size==0u) { return; }
        ::fast_io::io::print(out_,::fast_io::mnp::strvw(bytes,bytes+size)); position_ += size;
    }
    ::std::uint64_t current_pos() const override { return position_; }
public:
    explicit fast_io_ir_stream(::fast_io::native_io_observer out) : ::llvm::raw_ostream{true},out_{out} {}
};
static bool flag_query(void const* context, ::std::size_t index, bool& result) noexcept
{
    if(context == nullptr || index >= 8u) { return false; }
    result = (*static_cast<::std::array<bool,8u> const*>(context))[index]; return true;
}
static ::std::size_t instructions(::llvm::Function const& function)
{
    ::std::size_t result{}; for(auto const& block:function) { result += block.size(); } return result;
}

int main(int argc,char** argv)
{
    require(argc==1 || argc==2 || argc==3 || argc==5,"optional IR/bitcode and target pointer bits/byte order");
    unsigned pointer_bits{static_cast<unsigned>(sizeof(::std::uintptr_t)*8u)};
    bool big_endian{::std::endian::native == ::std::endian::big};
    if(argc==5)
    {
        ::fast_io::cstring_view const text{::fast_io::mnp::os_c_str(argv[3])};
        auto const parsed{::fast_io::parse_by_scan(text.data(),text.data()+text.size(),pointer_bits)};
        require(parsed.code==::fast_io::parse_code::ok && parsed.iter==text.data()+text.size() &&
            (pointer_bits==32u || pointer_bits==64u),"exact 32/64 target pointer layout");
        ::fast_io::cstring_view const order{::fast_io::mnp::os_c_str(argv[4])};
        require(order=="little" || order=="big","exact target byte order");
        big_endian=order=="big";
    }
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(),"actual native LLVM target");
    auto context{::std::make_unique<::llvm::LLVMContext>()};
    auto module{::std::make_unique<::llvm::Module>("actual-private-snapshot-copy",*context)};
    module->setDataLayout(pointer_bits==64u ? (big_endian ? "E-p:64:64" : "e-p:64:64") :
        (big_endian ? "E-p:32:32" : "e-p:32:32"));
    auto const signature{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(*context),false)};
    auto const function{::llvm::Function::Create(signature,::llvm::GlobalValue::ExternalLinkage,"main",*module)};
    auto const entry{::llvm::BasicBlock::Create(*context,"entry",function)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder=context.get(); state.llvm_module=module.get(); state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    auto& ir{*state.ir_builder};
    constexpr ::std::array types{emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::i64,
        emit::runtime_operand_stack_value_type::f32,emit::runtime_operand_stack_value_type::f64,
        emit::runtime_operand_stack_value_type::v128,emit::runtime_operand_stack_value_type::funcref,
        emit::runtime_operand_stack_value_type::externref,emit::runtime_operand_stack_value_type::funcref};
    constexpr ::std::array<::std::uint64_t,8u> bits{0x12345678u,0x123456789abcdef0u,0x7f812345u,0x7ff0123456789abcu,
        0x1020304050607080u,0x12345u,0u,0u};
    for(::std::size_t i{};i!=types.size();++i)
    {
        state.local_types.push_back(types[i]);
        auto const type{emit::get_llvm_type_from_wasm_value_type(*context,types[i])};
        auto const local{emit::create_llvm_jit_entry_block_alloca(ir,type,nullptr,"actual.local")};
        state.local_pointers.push_back(local);
        if(i == 7u) { continue; } // genuine uninitialized storage; must never be loaded
        auto const integer{::llvm::Type::getIntNTy(*context,static_cast<unsigned>(emit::get_runtime_wasm_value_type_abi_size(types[i])*8u))};
        auto const value{::llvm::ConstantInt::get(integer,bits[i])};
        // Initialize real typed storage through an integer bit carrier. No
        // floating conversion or producer-specific NaN text format is needed.
        ir.CreateStore(value,local);
    }
    constexpr ::std::size_t payload{8u*16u}, packet{payload+8u};
    auto const output{emit::create_llvm_jit_entry_block_alloca(ir,::llvm::ArrayType::get(ir.getInt8Ty(),packet),nullptr,"output")};
    auto const flags_out{ir.CreateInBoundsGEP(ir.getInt8Ty(),output,ir.getInt64(payload))};
    ::std::array<bool,8u> mask{true,true,true,true,true,true,true,false};
    state.debug_local_initialization={::std::addressof(mask),flag_query};
    auto const flags{emit::prepare_runtime_local_func_llvm_jit_debug_snapshot_flags(state,8u)};
    require(flags != nullptr && emit::prepare_runtime_local_func_llvm_jit_debug_snapshot_flags(state,8u)==flags &&
        state.debug_local_snapshot_flags.size()==1u,"canonical flags reused, no per-op global");
    require(emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,8u),"actual entry table");
    auto const absent_address{ir.CreateInBoundsGEP(::llvm::ArrayType::get(ir.getPtrTy(),8u),
        state.snapshot_local_pointers,{ir.getInt32(0u),ir.getInt32(7u)})};
    // Negative DATA only: an absent local has a null candidate address. The
    // selected immutable zero storage must prevent ANY value dereference.
    ir.CreateStore(::llvm::ConstantPointerNull::get(ir.getPtrTy()),absent_address);
    require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,8u,output,flags_out,flags,true),"actual public copier");
    ::llvm::Value* ok{ir.getTrue()};
    auto check{[&](::llvm::Value* condition) { ok=ir.CreateAnd(ok,condition); }};
    auto cell{[&](::std::size_t offset) { return ir.CreateInBoundsGEP(ir.getInt8Ty(),output,ir.getInt64(offset)); }};
    for(::std::size_t i{};i!=8u;++i)
    {
        auto const flag{ir.CreateLoad(ir.getInt8Ty(),cell(payload+i))}; flag->setAlignment(::llvm::Align{1u});
        check(ir.CreateICmpEQ(flag,ir.getInt8(i==7u ? 0u:1u)));
        if(i==5u || i==6u)
        {
            auto const value{ir.CreateLoad(ir.getInt8Ty(),cell(i*16u))}; value->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(value,ir.getInt8(i==6u ? 1u:0u))); // nullness only
        }
        else if(i!=7u)
        {
            auto const integer{::llvm::Type::getIntNTy(*context,static_cast<unsigned>(emit::get_runtime_wasm_value_type_abi_size(types[i])*8u))};
            auto const value{ir.CreateLoad(integer,cell(i*16u))}; value->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(value,::llvm::ConstantInt::get(integer,bits[i])));
        }
        if(i==7u)
        {
            auto const value{ir.CreateLoad(ir.getInt128Ty(),cell(i*16u))}; value->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(value,::llvm::ConstantInt::get(ir.getInt128Ty(),0u)));
        }
    }
    auto const public_copy{state.debug_local_snapshot_copy};
    require(public_copy != nullptr && public_copy->getSubprogram()==nullptr &&
        !public_copy->hasFnAttribute("uwvm.debug.activation") &&
        public_copy->hasFnAttribute(::llvm::Attribute::NoInline),"private instrumentation has no Wasm/ASM scope");
    require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,8u,output,flags_out,flags,false),"actual raw checkpoint copier");
    auto const raw_type{emit::get_llvm_type_from_wasm_value_type(*context,types[5u])};
    auto const raw{ir.CreateLoad(raw_type,cell(5u*16u))}; raw->setAlignment(::llvm::Align{1u});
    check(ir.CreateICmpEQ(raw,::llvm::ConstantInt::get(raw_type,bits[5u])));
    // A later real store is visible immediately; no stop/activation cache is substituted.
    ir.CreateStore(ir.getInt32(42u),state.local_pointers[0u]);
    ir.CreateStore(::llvm::ConstantInt::get(raw_type,9u),state.local_pointers[7u]);
    ir.CreateStore(state.local_pointers[7u],absent_address);
    mask[7u]=true;
    auto const updated_flags{emit::prepare_runtime_local_func_llvm_jit_debug_snapshot_flags(state,8u)};
    require(updated_flags != flags && state.debug_local_snapshot_flags.size()==2u,"changed init mask stays distinct");
    require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,8u,output,flags_out,updated_flags,true) &&
        state.debug_local_snapshot_copy==public_copy,"same copier observes actual changed values");
    auto const changed{ir.CreateLoad(ir.getInt32Ty(),cell(0u))};changed->setAlignment(::llvm::Align{1u});
    check(ir.CreateICmpEQ(changed,ir.getInt32(42u)));
    auto const present{ir.CreateLoad(ir.getInt8Ty(),cell(payload+7u))};present->setAlignment(::llvm::Align{1u});
    check(ir.CreateICmpEQ(present,ir.getInt8(1u)));
    auto const not_null{ir.CreateLoad(ir.getInt8Ty(),cell(7u*16u))};not_null->setAlignment(::llvm::Align{1u});
    check(ir.CreateICmpEQ(not_null,ir.getInt8(0u)));
    auto const before_invalid{instructions(*function)};
    require(!emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,0u) &&
        !emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,257u) &&
        instructions(*function)==before_invalid,"zero/oversized descriptors rejected before IR");
    auto const other{::llvm::Function::Create(signature,::llvm::GlobalValue::InternalLinkage,"foreign",*module)};
    ::llvm::IRBuilder<> foreign{::llvm::BasicBlock::Create(*context,"entry",other)};
    auto const pointer{foreign.CreateAlloca(ir.getInt32Ty())};foreign.CreateRet(foreign.getInt32(0u));
    auto const saved{state.local_pointers[0u]};state.local_pointers[0u]=pointer;
    require(!emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,8u) &&
        instructions(*function)==before_invalid,"foreign function local never enters table");
    state.local_pointers[0u]=saved;
    auto const count{state.snapshot_local_pointers_count};state.snapshot_local_pointers_count=7u;
    require(!emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,8u),"wrong table count rejected");
    state.snapshot_local_pointers_count=count;
    ir.CreateRet(ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));

    // The emitted parent grows with number of sites, not locals * sites.
    auto const stress{::llvm::Function::Create(::llvm::FunctionType::get(ir.getVoidTy(),{ir.getPtrTy()},false),
        ::llvm::GlobalValue::ExternalLinkage,"snapshot_stress",*module)};
    emit::runtime_local_func_llvm_jit_emit_state_t many{};
    many.llvm_context_holder=context.get();many.llvm_module=module.get();many.llvm_function=stress;
    many.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",stress));
    auto& large{*many.ir_builder};
    for(::std::size_t i{};i!=256u;++i)
    {
        many.local_types.push_back(emit::runtime_operand_stack_value_type::i32);
        auto const p{emit::create_llvm_jit_entry_block_alloca(large,large.getInt32Ty(),nullptr,"local")};
        many.local_pointers.push_back(p);large.CreateStore(large.getInt32(static_cast<unsigned>(i)),p);
    }
    constexpr ::std::array<::std::uint8_t,256u> zero_flags{};
    auto const ones{new ::llvm::GlobalVariable(*module,::llvm::ArrayType::get(large.getInt8Ty(),256u),true,
        ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantDataArray::get(*context,::llvm::ArrayRef<::std::uint8_t>{zero_flags}),"stress.flags")};
    // Output is unused; zero flags deliberately exercise the no-read branches.
    for(::std::size_t i{};i!=1024u;++i)
    {
        auto const saved_flags{large.CreateInBoundsGEP(large.getInt8Ty(),stress->getArg(0u),large.getInt64(256u*16u))};
        require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(many,256u,stress->getArg(0u),saved_flags,ones,(i&1u)==0u),"bounded repeated sites");
    }
    large.CreateRetVoid();
    require(many.debug_local_snapshot_copy==many.checkpoint_local_snapshot_copy &&
        many.debug_local_snapshot_copy->size()==1u,"one scalar public/raw body with one IR block");
    auto const before_bad_kind{instructions(*stress)};
    many.debug_local_snapshot_copy->removeFnAttr("uwvm.debug.private.local-copy.kind");
    require(emit::prepare_runtime_local_func_llvm_jit_snapshot_copy(many,256u,true)==nullptr &&
        instructions(*stress)==before_bad_kind,"missing cache kind rejected before IR without SDK assertion");
    many.debug_local_snapshot_copy->addFnAttr("uwvm.debug.private.local-copy.kind","numeric");
    auto const main_return{::llvm::cast<::llvm::ReturnInst>(function->getEntryBlock().getTerminator())};
    ir.SetInsertPoint(main_return);
    auto const stress_output{emit::create_llvm_jit_entry_block_alloca(ir,
        ::llvm::ArrayType::get(ir.getInt8Ty(),256u*17u),nullptr,"stress.output")};
    ir.CreateCall(stress,{stress_output});
    for(auto const offset : {0u,255u*16u})
    {
        auto const location{ir.CreateInBoundsGEP(ir.getInt8Ty(),stress_output,ir.getInt64(offset))};
        auto const value{ir.CreateLoad(ir.getInt128Ty(),location)};value->setAlignment(::llvm::Align{1u});
        check(ir.CreateICmpEQ(value,::llvm::ConstantInt::get(ir.getInt128Ty(),0u)));
    }
    auto const last_flag{ir.CreateLoad(ir.getInt8Ty(),
        ir.CreateInBoundsGEP(ir.getInt8Ty(),stress_output,ir.getInt64(256u*16u+255u)))};
    last_flag->setAlignment(::llvm::Align{1u});check(ir.CreateICmpEQ(last_flag,ir.getInt8(0u)));
    main_return->setOperand(0u,ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));

    require(instructions(*stress)<4000u && instructions(*many.debug_local_snapshot_copy)<6000u,
        "IR work shared across 1024 sites and 256 locals");
    require(!::llvm::verifyModule(*module),"complete actual LLVM module verifies");
    ::fast_io::io::println("snapshot IR: sites=1024 locals=256 parent-instructions=",instructions(*stress),
        " shared-copy-instructions=",instructions(*many.debug_local_snapshot_copy));
    if(argc>=2)
    {
        ::fast_io::native_file output_file{::fast_io::mnp::os_c_str(argv[1]),::fast_io::open_mode::out};
        fast_io_ir_stream stream{output_file};module->print(stream,nullptr);stream.flush();
    }
    if(argc>=3)
    {
        ::fast_io::native_file output_file{::fast_io::mnp::os_c_str(argv[2]),::fast_io::open_mode::out};
        fast_io_ir_stream stream{output_file};::llvm::WriteBitcodeToFile(*module,stream);stream.flush();
    }
    if(argc==5)
    {
        ::fast_io::io::println("PASS actual production copy IR DATA; pointer-bits=",pointer_bits,
            " byte-order=",::fast_io::mnp::os_c_str(big_endian ? "big" : "little"),"; target execution remains required");
        return 0;
    }
    ::llvm::Triple const triple{::llvm::sys::getProcessTriple()};
    ::llvm::EngineBuilder select{};select.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
    support::llvm_jit_mcjit_configure_code_model(select,triple);support::llvm_jit_mcjit_configure_host_unwind_abi(select);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{select.selectTarget()}};
    require(bool(target),"actual target machine");module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()).create(target.release())}};
    require(bool(engine),"real native MCJIT");engine->finalizeObject();require(!engine->hasError(),"actual code materialization");
    auto const address{engine->getFunctionAddress("main")};require(address!=0u,"actual native component entry");
    require(reinterpret_cast<int(*)()>(static_cast<::std::uintptr_t>(address))()==0,"native snapshot bytes/flags/updates/no-read checks");
    ::fast_io::io::println("PASS shared snapshot copy native DATA, init/bit/ref/owner/linear IR; no Wasm/ASM authority");
}

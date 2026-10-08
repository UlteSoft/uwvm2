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
// Complete numeric declarations and legacy public prefixes have different
// destination extents. Exercise real copied bytes, a live update, a sentinel,
// both publication orders, invalid extents and native MCJIT execution.
static void run_order(bool raw_first)
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(),"native target");
    auto context{::std::make_unique<::llvm::LLVMContext>()};
    auto module{::std::make_unique<::llvm::Module>("large-private-snapshot-copy",*context)};
    ::llvm::Triple const triple{::llvm::sys::getProcessTriple()};
    ::llvm::EngineBuilder select{};select.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
    support::llvm_jit_mcjit_configure_code_model(select,triple);support::llvm_jit_mcjit_configure_host_unwind_abi(select);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{select.selectTarget()}};
    require(bool(target),"target machine");module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    ::llvm::IRBuilder<> types{*context};
    constexpr ::std::size_t count{10000u}, prefix{256u};
    constexpr auto bytes{count * (::uwvm2::runtime::checkpoint::observer_local_metadata_bytes + 16u)};
    auto const workspace_type{::llvm::ArrayType::get(types.getInt8Ty(),bytes)};
    auto const workspace{new ::llvm::GlobalVariable(*module,workspace_type,false,::llvm::GlobalValue::PrivateLinkage,
        ::llvm::ConstantAggregateZero::get(workspace_type),"owned.component.workspace")};
    auto const integer{types.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*8u))};
    auto const allocate{::llvm::Function::Create(::llvm::FunctionType::get(integer,{integer},false),
        ::llvm::GlobalValue::InternalLinkage,"component.allocate",*module)};
    ::llvm::IRBuilder<> allocation{::llvm::BasicBlock::Create(*context,"entry",allocate)};
    allocation.CreateRet(allocation.CreatePtrToInt(workspace,integer));
    auto const function{::llvm::Function::Create(::llvm::FunctionType::get(types.getInt32Ty(),false),
        ::llvm::GlobalValue::ExternalLinkage,"main",*module)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder=context.get();state.llvm_module=module.get();state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",function));
    auto& ir{*state.ir_builder};
    auto const owner{ir.CreateCall(allocate,{::llvm::ConstantInt::get(integer,bytes)})};
    state.checkpoint_observer_workspace_allocation=owner;
    state.checkpoint_observer_workspace=ir.CreateIntToPtr(owner,ir.getPtrTy());
    for(::std::size_t i{};i!=count;++i)
    {
        state.local_types.push_back(emit::runtime_operand_stack_value_type::i64);
        auto const local{emit::create_llvm_jit_entry_block_alloca(ir,ir.getInt64Ty(),nullptr,"actual.local")};
        state.local_pointers.push_back(local);ir.CreateStore(ir.getInt64(0x1234567890000000ull+i),local);
    }
    ::std::array<::std::uint8_t,count> mask{};mask.fill(1u);mask[7u]=0u;
    auto const flags{new ::llvm::GlobalVariable(*module,::llvm::ArrayType::get(ir.getInt8Ty(),count),true,
        ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantDataArray::get(*context,::llvm::ArrayRef<::std::uint8_t>{mask}),"actual.flags")};
    auto const public_output{emit::create_llvm_jit_entry_block_alloca(ir,::llvm::ArrayType::get(ir.getInt8Ty(),prefix*17u+16u),nullptr,"public.output")};
    auto const raw_output{emit::create_llvm_jit_entry_block_alloca(ir,::llvm::ArrayType::get(ir.getInt8Ty(),count*17u+16u),nullptr,"raw.output")};
    auto cell{[&](::llvm::Value* output,::std::size_t offset) { return ir.CreateInBoundsGEP(ir.getInt8Ty(),output,ir.getInt64(offset)); }};
    constexpr ::std::uint64_t sentinel{0xabcdef0123456789ull};
    ir.CreateStore(ir.getInt64(sentinel),cell(public_output,prefix*17u))->setAlignment(::llvm::Align{1u});
    ir.CreateStore(ir.getInt64(sentinel),cell(raw_output,count*17u))->setAlignment(::llvm::Align{1u});
    auto copy{[&](bool raw)
    {
        auto const n{raw ? count : prefix};auto const output{raw ? raw_output : public_output};
        require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,n,output,cell(output,n*16u),flags,!raw),"real complete/prefix copier");
    }};
    copy(raw_first);copy(!raw_first);
    require(state.debug_local_snapshot_copy!=state.checkpoint_local_snapshot_copy &&
        state.debug_local_snapshot_copy_count==prefix && state.checkpoint_local_snapshot_copy_count==count &&
        state.snapshot_local_pointers_count==count,"different extents retain distinct bodies and one complete table");
    auto const pointer_table{::llvm::dyn_cast<::llvm::GetElementPtrInst>(state.snapshot_local_pointers)};
    require(pointer_table!=nullptr && pointer_table->getPointerOperand()==state.checkpoint_observer_workspace,
        "complete addresses stay in owned heap, no large native table");
    ::llvm::Value* ok{ir.getTrue()};
    auto check{[&](::llvm::Value* condition) { ok=ir.CreateAnd(ok,condition); }};
    auto check64{[&](::llvm::Value* output,::std::size_t offset,::std::uint64_t expected)
    {
        auto const value{ir.CreateLoad(ir.getInt64Ty(),cell(output,offset))};value->setAlignment(::llvm::Align{1u});
        check(ir.CreateICmpEQ(value,ir.getInt64(expected)));
    }};
    check64(public_output,255u*16u,0x12345678900000ffull);
    check64(raw_output,(count-1u)*16u,0x1234567890000000ull+(count-1u));
    check64(public_output,7u*16u,0u);check64(raw_output,7u*16u,0u);
    check64(public_output,prefix*17u,sentinel);check64(raw_output,count*17u,sentinel);
    ir.CreateStore(ir.getInt64(0xfedcba9876543210ull),state.local_pointers[count-1u]);
    copy(true);copy(false);
    check64(raw_output,(count-1u)*16u,0xfedcba9876543210ull);
    check64(public_output,prefix*17u,sentinel);check64(raw_output,count*17u,sentinel);
    auto const saved_count{state.checkpoint_local_snapshot_copy_count};state.checkpoint_local_snapshot_copy_count=prefix;
    require(emit::prepare_runtime_local_func_llvm_jit_snapshot_copy(state,count,false)==nullptr,"wrong cached extent rejected");
    state.checkpoint_local_snapshot_copy_count=saved_count;
    owner->setArgOperand(0u,::llvm::ConstantInt::get(integer,bytes-1u));
    require(!emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,count),"short private heap rejected before copy");
    owner->setArgOperand(0u,::llvm::ConstantInt::get(integer,bytes));
    auto const before{function->getEntryBlock().size()};
    require(!emit::prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,count+1u) &&
        function->getEntryBlock().size()==before,"oversized declarations rejected before IR");
    ir.CreateRet(ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));
    require(!::llvm::verifyModule(*module),"complete module verifies");
    for(auto const copier:{state.debug_local_snapshot_copy,state.checkpoint_local_snapshot_copy})
    {
        require(copier->hasLocalLinkage() && copier->hasFnAttribute(::llvm::Attribute::NoInline) &&
            copier->getSubprogram()==nullptr && !copier->hasFnAttribute("uwvm.debug.activation"),"private ASM-hidden copier");
    }
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()).create(target.release())}};
    require(bool(engine),"actual native MCJIT");engine->finalizeObject();require(!engine->hasError(),"materialization");
    auto const address{engine->getFunctionAddress("main")};require(address!=0u,"native entry");
    require(reinterpret_cast<int(*)()>(static_cast<::std::uintptr_t>(address))()==0,"full/prefix bytes, updates, absent flags, sentinels");
    ::fast_io::io::println("PASS large snapshot DATA: locals=10000 prefix=256 raw-first=",raw_first,
        " complete-tail=9999 sentinel=preserved; no Wasm/ASM authority");
}
int main() { run_order(true);run_order(false); }

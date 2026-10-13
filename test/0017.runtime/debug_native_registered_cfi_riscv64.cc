// Real RV64 TailCC + production memory manager + relocated registered CFI.
// Metadata only: no Wasm trap, stack read, caller or finish authority is minted.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/TargetSelect.h>
#include <fast_io.h>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native registered CFI failure line=", __LINE__); return 1; } } while(false)

struct symbols final : ::llvm::JITEventListener
{
    struct extent { ::std::uintptr_t begin{}, end{}; };
    ::std::array<extent, 2u> actual{};
    bool valid{true};
    void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        for(auto const& [symbol, size]: ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); valid = false; return; }
            unsigned index{};
            if(*name == "uwvm_cfi_leaf") { index = 0u; }
            else if(*name == "uwvm_cfi_caller") { index = 1u; }
            else { continue; }
            auto address{symbol.getAddress()}; auto section{symbol.getSection()};
            if(!address || !section)
            {
                if(!address) { ::llvm::consumeError(address.takeError()); }
                if(!section) { ::llvm::consumeError(section.takeError()); }
                valid = false; return;
            }
            if(*section == object.section_end() || size == 0u || *address < (*section)->getAddress()) { valid = false; return; }
            auto const base{loaded.getSectionLoadAddress(**section)};
            auto const offset{*address - (*section)->getAddress()};
            constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            if(base == 0u || base > limit || offset > limit - base || size > limit - base - offset || actual[index].begin != 0u)
            { valid = false; return; }
            actual[index] = {static_cast<::std::uintptr_t>(base + offset), static_cast<::std::uintptr_t>(base + offset + size)};
        }
    }
};


int main(int argc, char const* const* argv)
{
#if !defined(__riscv) || __riscv_xlen != 64 || !defined(__linux__)
 return 77;
#else
 unsigned optimization{};CHECK(argc==1 || argc==2);
 if(argc==2)
 {
  auto const end{argv[1]+::std::strlen(argv[1])};
  auto const parsed{::fast_io::parse_by_scan(argv[1],end,::fast_io::mnp::dec_get<true,true>(optimization))};
  CHECK(parsed.code==::fast_io::parse_code::ok && parsed.iter==end);
 }
 CHECK(optimization==0u || optimization==3u);
 CHECK(!::llvm::InitializeNativeTarget());CHECK(!::llvm::InitializeNativeTargetAsmPrinter());CHECK(!::llvm::InitializeNativeTargetAsmParser());
 ::llvm::LLVMContext context{};auto module{::std::make_unique<::llvm::Module>("rv_epilogue_probe",context)};
 auto i64{::llvm::Type::getInt64Ty(context)};
 ::std::array<::llvm::Type*,64u> types{};types.fill(i64);
 auto type{::llvm::FunctionType::get(i64,types,false)};
 auto leaf{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_leaf",*module)};
 auto caller{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_caller",*module)};
 for(auto f:{leaf,caller}){f->setUWTableKind(::llvm::UWTableKind::Async);f->setCallingConv(::llvm::CallingConv::Tail);f->addFnAttr(::llvm::Attribute::NoInline);}
 ::llvm::IRBuilder<> builder{::llvm::BasicBlock::Create(context,"entry",leaf)};
 builder.CreateRet(builder.CreateAdd(leaf->getArg(63u),builder.getInt64(1)));
 builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",caller));
 auto slot{builder.CreateAlloca(i64,builder.getInt32(128))};builder.CreateStore(caller->getArg(0u),slot,true);
 ::std::array<::llvm::Value*,64u> args{};for(unsigned i{};i<64u;++i)args[i]=caller->getArg(i);
 auto call{builder.CreateCall(leaf,args)};call->setCallingConv(::llvm::CallingConv::Tail);
 builder.CreateRet(builder.CreateAdd(call,builder.CreateLoad(i64,slot,true)));CHECK(!::llvm::verifyModule(*module));
 auto manager{::std::make_unique<cfi::runtime_llvm_jit_section_memory_manager>()};auto observer{manager.get()};
 symbols loaded{};auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder(::std::move(module)).setEngineKind(::llvm::EngineKind::JIT).setOptLevel(optimization==3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None).setMCJITMemoryManager(::std::move(manager)).create()}};
 CHECK(engine);engine->RegisterJITEventListener(&loaded);engine->finalizeObject();CHECK(!observer->has_finalization_failure()&&loaded.valid);
 unsigned cfa_only{},full_rows{}; bool frame_pointer_cfa{};
 ::std::int32_t incoming_area{};
 for(unsigned n{};n<2u;++n)
 {
  auto const f{loaded.actual[n]};CHECK(f.end>f.begin);
  cfi::native_debug_cfa_row entry{};
  CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,entry) &&
        entry.usable && entry.cfa_register==2u && entry.cfa_offset>0 && entry.cfa_offset%8==0);
  if(n==0u) { incoming_area=entry.cfa_offset; }
  else { CHECK(entry.cfa_offset==incoming_area); }
  for(auto pc{f.begin};pc<f.end;++pc)
  {
   cfi::native_debug_cfa_row scalar{};
   CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && scalar.usable &&
         scalar.begin<=pc && scalar.end>pc &&
         (scalar.cfa_register==2u || scalar.cfa_register==8u));
   frame_pointer_cfa = frame_pointer_cfa || scalar.cfa_register==8u;
   cfi::native_debug_cfi_row row{};
   if(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row))
   {
    ++full_rows;
    CHECK(row.usable && row.cfa_usable && row.cfa_register==scalar.cfa_register &&
          row.cfa_offset==scalar.cfa_offset && row.begin==scalar.begin && row.end==scalar.end);
   }
   else { ++cfa_only;CHECK(!row.usable && row.begin==0u); }
  }
  // Sixty-four i64 arguments require an incoming stack area on RV64.
  // The actual TailCC CFI includes this area in the entry CFA. After the
  // callee pops it, the return instruction has CFA=SP, not SP minus that area.
  // Restored RA has no retained rule: CFA-only metadata cannot grant a caller.
  cfi::native_debug_cfa_row epilogue{};
  CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.end-1u,epilogue) &&
        epilogue.usable && epilogue.cfa_register==2u && epilogue.cfa_offset==0);
  ::fast_io::io::println("actual RV64 TailCC function=",n," incoming-stack-bytes=",incoming_area,
                        " return-cfa-is-sp=true");
  cfi::native_debug_cfi_row caller_row{};
  CHECK(!observer->copy_debug_native_cfi_row(f.begin,f.end,f.end-1u,caller_row) && !caller_row.usable);
  CHECK(!observer->copy_debug_native_cfa_row(f.begin-1u,f.end,f.begin,epilogue) && !epilogue.usable);
  CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end+1u,f.begin,epilogue) && !epilogue.usable);
  CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.end,epilogue) && !epilogue.usable);
 }
 CHECK(cfa_only!=0u && full_rows!=0u && frame_pointer_cfa);
 observer->deregisterEHFrames();
 for(unsigned n{};n<2u;++n)
 {
  auto const f{loaded.actual[n]};cfi::native_debug_cfa_row row{};
  CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,row) && !row.usable);
 }
 engine->UnregisterJITEventListener(&loaded);
 ::fast_io::io::println("actual RV64 TailCC registered CFI: PASS optimization=",optimization,
  " cfa-only-bytes=",cfa_only," full-rule-bytes=",full_rows,
  " physical-caller-qualified=false native-finish-qualified=false");
#endif
}

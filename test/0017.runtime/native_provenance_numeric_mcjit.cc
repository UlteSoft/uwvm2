// Actual MCJIT loaded-object/lifetime COMPONENT. It creates/finalizes native
// code in the keeper's cgroup but never calls a generated function. Actual VM
// capture/trap credentials and product fresh-TU qualification are separate.
#define UWVM_RUNTIME_LLVM_JIT 1
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h>
#include <uwvm2/uwvm/debugger/native_disassembly_abi.h>
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_function_address.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <fast_io.h>

namespace metadata = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;
namespace provenance = ::uwvm2::runtime::lib::details::native_loaded_provenance;
namespace abi = ::uwvm2::uwvm::debugger::native_disassembly_abi;
#include "native_stackmap_numeric_boundary_test.h"
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native provenance MCJIT owner failure line=", __LINE__); return 1; } } while(false)

struct native_owner
{
    // Reverse destruction retires actual executable engine before context/rows.
    provenance::image rows{};
    ::std::unique_ptr<::llvm::LLVMContext> context{};
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{};
    ::std::uintptr_t begin{}, end{}, sample{};
};

// Optional bounded diagnostic capture uses LLVM's required stream parameter
// and fast_io for filesystem writes. It never calls the generated function.
static char const* diagnostic_directory{};
struct diagnostic_listener : ::llvm::JITEventListener
{
    unsigned kind{}, generation{}, units{}, private_units{}, sequences{}, owned_sequences{}, line_rows{};
    bool malformed{}, stackmap_boundary_passed{true};
    void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        if(generation==4u) { stackmap_boundary_passed=check_actual_stackmap_boundary(object,loaded,kind); }
        auto const error{[&](::llvm::Error failure) { malformed = true; ::llvm::consumeError(::std::move(failure)); }};
        auto relocated{::llvm::DWARFContext::create(object, ::llvm::DWARFContext::ProcessDebugRelocations::Process,
            ::std::addressof(loaded), "", error, error)};
        for(auto const& unit: relocated->compile_units())
        {
            ++units; auto const die{unit->getUnitDIE(false)};
            if(::llvm::dwarf::toStringRef(die.find(::llvm::dwarf::DW_AT_producer)) != "uwvm debug-jit native Wasm provenance v1") { continue; }
            ++private_units; auto table{relocated->getLineTableForUnit(unit.get(), error)};
            if(!table) { error(table.takeError()); continue; }
            if(*table == nullptr) { continue; }
            line_rows += (*table)->Rows.size();
            for(auto const& sequence: (*table)->Sequences)
            {
                ++sequences; unsigned owners{};
                for(auto const& section: object.sections())
                {
                    if(!section.isText() || section.getSize() == 0u) { continue; }
                    auto const begin{loaded.getSectionLoadAddress(section)}, size{section.getSize()};
                    if(begin != 0u && size <= UINTPTR_MAX - begin && sequence.LowPC >= begin && sequence.HighPC <= begin + size &&
                       (sequence.SectionIndex == ::llvm::object::SectionedAddress::UndefSection || sequence.SectionIndex == section.getIndex())) { ++owners; }
                }
                if(owners == 1u) { ++owned_sequences; }
            }
        }
        // Aggregate evidence about this component's own emitted object only.
        // No native address, register value, stack or VM code is printed.
        ::fast_io::io::println("loaded metadata kind=", kind, " malformed=", malformed, " units=", units,
            " private-units=", private_units, " rows=", line_rows, " sequences=", sequences, " owned-sequences=", owned_sequences);
        if(diagnostic_directory == nullptr) { return; }
        auto const path{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(diagnostic_directory), "/numeric-", kind, "-g", generation, ".dwarf")};
        auto dwarf{::llvm::DWARFContext::create(object)};
        ::std::string text{}; ::llvm::raw_string_ostream buffer{text};
        ::llvm::DIDumpOptions options{}; options.ShowChildren = true; dwarf->dump(buffer, options);
        ::fast_io::native_file output{path, ::fast_io::open_mode::out};
        ::fast_io::io::print(output, ::fast_io::mnp::strvw(text));
    }
};
static bool build(native_owner& owner, unsigned generation, bool debug, unsigned kind = 0x7fu, bool require_numeric = true, bool constant_vector = false, bool owned_branch = false)
{
    owner.context = ::std::make_unique<::llvm::LLVMContext>();
    auto module{::std::make_unique<::llvm::Module>("actual-loaded-native-provenance", *owner.context)};
    ::llvm::Type* integer{};
    switch(kind)
    {
        case 0x7fu: integer = ::llvm::Type::getInt32Ty(*owner.context); break;
        case 0x7eu: integer = ::llvm::Type::getInt64Ty(*owner.context); break;
        case 0x7du: integer = ::llvm::Type::getFloatTy(*owner.context); break;
        case 0x7cu: integer = ::llvm::Type::getDoubleTy(*owner.context); break;
        case 0x7bu: integer = ::llvm::FixedVectorType::get(::llvm::Type::getInt8Ty(*owner.context), 16u); break;
        default: return false;
    }
    auto* signature{::llvm::FunctionType::get(integer, {integer}, false)};
    auto* function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "native_provenance_probe", *module)};
    auto const identity{::fast_io::concat_fast_io("uwvm-m2-f3-g", ::fast_io::mnp::dec(generation), ".wasm-native-v1")};
    ::std::string_view const identity_view{identity.data(), identity.size()};
    auto* scope{debug ? metadata::attach(*function, {identity.data(), identity.size()}) : nullptr};
    if(debug && scope == nullptr) { return false; }
    auto* block{::llvm::BasicBlock::Create(*owner.context, "entry", function)};
    ::llvm::IRBuilder<> builder{block};
    ::llvm::SmallVector<metadata::numeric_identity, 0u> identities{};
    // Exercise the same materializer + consuming witness as actual -Rdbg
    // code. In particular, an empty vector INLINEASM return previously lost
    // every v128 DBG_VALUE even though the emitted register was real.
    auto const record{[&](::llvm::Value* value, ::std::size_t ordinal) -> bool
    {
        if(!metadata::numeric(builder, value, kind, ordinal, kind == 0x7bu)) { return false; }
        ::llvm::Instruction* witness{};
        auto* const actual{metadata::materialize_numeric_register(builder, value, &witness)};
        // Match the runtime: unsupported whole-register classes keep ordinary
        // DWARF. i686 i64 now has a complete bit-preserving XMM witness.
        if(actual == nullptr || !metadata::numeric(builder, actual, kind, ordinal, true, witness)) { return false; }
        if(kind == 0x7bu && actual != value)
        {
            auto* const semantic{::llvm::dyn_cast<::llvm::Instruction>(builder.CreateBitCast(actual, value->getType()))};
            if(semantic == nullptr) { return false; }
            identities.push_back({value, semantic});
        }
        return true;
    }};
    if(debug && !metadata::location(builder, scope, 0u, 3u)) { return false; }
    ::llvm::Value* input{function->getArg(0u)};
    if(constant_vector)
    {
        if(!integer->isVectorTy()) { return false; }
        input = ::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(0xa5u));
    }
    if(debug && !record(input, 0u)) { return false; }
    if(owned_branch)
    {
        if(!integer->isIntegerTy()) { return false; }
        auto* const left{::llvm::BasicBlock::Create(*owner.context,"numeric.left",function)};
        auto* const right{::llvm::BasicBlock::Create(*owner.context,"numeric.right",function)};
        auto* const merge{::llvm::BasicBlock::Create(*owner.context,"numeric.merge",function)};
        builder.CreateCondBr(builder.CreateICmpEQ(input,::llvm::ConstantInt::get(integer,0u)),left,right);
        builder.SetInsertPoint(left);
        auto* const a{builder.CreateXor(input,::llvm::ConstantInt::get(integer,1u))};builder.CreateBr(merge);
        builder.SetInsertPoint(right);
        auto* const b{builder.CreateXor(input,::llvm::ConstantInt::get(integer,2u))};builder.CreateBr(merge);
        builder.SetInsertPoint(merge);
        auto* const phi{builder.CreatePHI(integer,2u)};phi->addIncoming(a,left);phi->addIncoming(b,right);input=phi;
        if(debug && !record(input,0u)) { return false; }
    }
    auto const operation{[&](::llvm::Value* input) -> ::llvm::Value*
    {
        if(integer->isFloatingPointTy()) { return builder.CreateFAdd(input, ::llvm::ConstantFP::get(integer, 9.0)); }
        if(integer->isVectorTy()) { return builder.CreateXor(input, ::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(9u))); }
        return builder.CreateXor(input, ::llvm::ConstantInt::get(integer, 9u));
    }};
    auto* add{operation(input)};
    if(debug && !record(add, 1u)) { return false; }
    if(debug && !metadata::location(builder, scope, 2u, 3u)) { return false; }
    if(debug && !record(add, 0u)) { return false; }
    ::llvm::Value* multiply{};
    if(integer->isFloatingPointTy()) { multiply = builder.CreateFMul(add, ::llvm::ConstantFP::get(integer, 7.0)); }
    else if(integer->isVectorTy()) { multiply = builder.CreateAdd(add, ::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(7u))); }
    else { multiply = builder.CreateMul(add, ::llvm::ConstantInt::get(integer, 7u)); }
    if(debug && !record(multiply, 1u)) { return false; }
    if(debug) { metadata::unknown(builder, scope); }
    builder.CreateRet(multiply);
    if(::llvm::verifyModule(*module)) { return false; }
    if(debug) { metadata::restrict_public_code(*function, identities); }
    if(::llvm::verifyModule(*module)) { return false; }
    // Use actual native CPU capabilities, as the runtime does. A generic
    // i686 target has no SSE class even when this test executable uses SSE2.
    ::llvm::SmallVector<::std::string,32u> attributes{};
    for(auto const& [name,enabled]: ::llvm::sys::getHostCPUFeatures())
    { attributes.emplace_back((enabled ? "+" : "-") + name.str()); }
    ::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_append_native_host_vector_features(
        ::llvm::Triple{::llvm::sys::getProcessTriple()},::llvm::sys::getHostCPUName(),attributes);
    if(::llvm::Triple{::llvm::sys::getProcessTriple()}.isMIPS())
    { attributes.emplace_back("+noabicalls");attributes.emplace_back("+long-calls"); }
    ::std::string error{};
    owner.engine.reset(::llvm::EngineBuilder(::std::move(module)).setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None).setErrorStr(&error)
        .setMCJITMemoryManager(::std::make_unique<::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>())
        .setMAttrs(attributes).setMCPU(::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_abi_host_cpu_name(
            ::llvm::Triple{::llvm::sys::getProcessTriple()}, ::llvm::sys::getHostCPUName())).create());
    if(!owner.engine) { return false; }
    ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges pending{*owner.engine, true};
    pending.configure_debug_full_capture(debug);
    diagnostic_listener diagnostic{}; diagnostic.kind = kind; diagnostic.generation = generation; owner.engine->RegisterJITEventListener(&diagnostic);
    owner.engine->finalizeObject();
    owner.engine->UnregisterJITEventListener(&diagnostic);
    if(owner.engine->hasError() || !diagnostic.stackmap_boundary_passed) { return false; }
    auto const address{owner.engine->getFunctionAddress("native_provenance_probe")};
    if(address == 0u || address > UINTPTR_MAX) { return false; }
    owner.begin = ::uwvm2::runtime::lib::details::native_function_code_address(static_cast<::std::uintptr_t>(address));
    pending.commit([](::std::uintptr_t, ::std::uintptr_t) noexcept {},
        [&](::std::uintptr_t begin, ::std::uintptr_t size) noexcept
        {
            if(begin == owner.begin && size != 0u && size <= UINTPTR_MAX - begin)
            { owner.end = begin + size; }
        });
    owner.rows = pending.take_debug_full_capture(7u);
    if(owner.end <= owner.begin || owner.end - owner.begin > 65536u) { return false; }
#if defined(__linux__) && defined(__mips__) && __SIZEOF_POINTER__ == 8
    if(owned_branch)
    {
        // Actual relocated function bytes owned by this component's engine.
        // No live trap, register, native stack or debugger read grant exists.
        ::std::array<::std::uint8_t,65536u> bytes{};
        ::std::memcpy(bytes.data(),reinterpret_cast<void const*>(owner.begin),owner.end-owner.begin);
        ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder decoder{};
        ::std::size_t branches{}, outside{};
        for(::std::size_t offset{};offset<owner.end-owner.begin;)
        {
            auto const decoded{decoder.decode(owner.begin+offset,{bytes.data()+offset,owner.end-owner.begin-offset})};
            if(!decoded || decoded.semantics().size==0u || decoded.semantics().size>owner.end-owner.begin-offset) { return false; }
            if(decoded.semantics().kind==::uwvm2::uwvm::debugger::native_instruction_semantics::flow::branch &&
               !decoded.semantics().indirect_branch)
            {
                if(!decoded.safe_for_same_owner_direct_branch() || !decoded.delayed_branch_pair()) { return false; }
                ++branches;auto const target{decoded.destination().display_pc};
                outside += target<owner.begin || target>=owner.end;
            }
            offset+=decoded.semantics().size;
        }
        if(branches==0u || (debug ? outside!=0u : outside==0u)) { return false; }
        ::fast_io::io::println("actual MIPS branch ownership debug=",debug," branches=",branches," outside=",outside,
            " generated-function-executed=false");
    }
#endif
    if(!debug) { return !owner.rows.valid() && owner.rows.row_count() == 0u; }
    if(!owner.rows.valid() || owner.rows.row_count() == 0u)
    { ::fast_io::io::perrln("loaded provenance rejected kind=", kind, " valid=", owner.rows.valid(), " rows=", owner.rows.row_count()); return false; }
    ::std::array<::std::uint8_t,65536u> permission{};
    if(!owner.rows.code_permissions(owner.begin, owner.end-owner.begin, owner.begin, owner.end, identity_view, 3u, 7u, permission.data())) { return false; }
    bool have_numeric{}, have_hidden{}, have_public_code{};
    for(auto pc{owner.begin}; pc < owner.end; ++pc)
    {
        struct location { unsigned dwarf_register{}, bits{}; }; location actual[64u]{};
        auto const count{owner.rows.numeric_locations(pc, owner.begin, owner.end, identity_view, 3u, 7u, actual, 64u)};
        location refused[64u]{};
        if(owner.rows.numeric_locations(pc, owner.begin, owner.end, "foreign.wasm-native-v1", 3u, 7u, refused, 64u) != 0u ||
           owner.rows.numeric_locations(pc, owner.begin, owner.end, identity_view, 3u, 8u, refused, 64u) != 0u ||
           owner.rows.numeric_locations(pc, owner.begin, owner.end, identity_view, 3u, 0u, refused, 64u) != 0u)
        { return false; }
        // The actual producer endpoint may omit a prologue/trailer while
        // remaining inside this same relocated DWARF subprogram. Narrowing
        // cannot create register or memory authority outside the native body.
        if(owner.end-owner.begin>2u && pc>owner.begin && pc+1u<owner.end)
        {
            auto const inner{owner.rows.numeric_locations(pc,owner.begin+1u,owner.end-1u,
                identity_view,3u,7u,refused,64u)};
            if(inner!=count) { return false; }
        }
        if(owner.begin>1u && owner.rows.numeric_locations(pc,owner.begin-1u,owner.end,
            identity_view,3u,7u,refused,64u)!=0u) { return false; }
        if(owner.end!=UINTPTR_MAX && owner.rows.numeric_locations(pc,owner.begin,owner.end+1u,
            identity_view,3u,7u,refused,64u)!=0u) { return false; }
        for(::std::size_t i{}; i != count; ++i)
        {
            auto const bits{kind == 0x7bu ? 128u : (kind == 0x7eu || kind == 0x7cu) ? 64u : 32u};
            if(actual[i].bits != bits) { return false; } have_numeric = true;
        }
        have_hidden |= permission[pc-owner.begin] == 0u;
        have_public_code |= permission[pc-owner.begin] == 1u;
    }
    // Whole-register locations can be unavailable after target legalization
    // (e.g. scalar-only targets' v128). Their pieces must stay
    // rejected; the real product separately requires its physical classes.
    if((require_numeric && !have_numeric) || !have_hidden || !have_public_code)
    { ::fast_io::io::perrln("numeric location coverage kind=", kind, " numeric=",have_numeric," required=",require_numeric," hidden=",have_hidden); return false; }
    for(auto pc{owner.begin}; pc < owner.end; ++pc)
    {
        auto const point{owner.rows.lookup(pc, owner.begin, owner.end, identity_view, 3u, 7u)};
        if(point.state == provenance::status::exact)
        {
            if((point.wasm_offset != 0u && point.wasm_offset != 2u) || point.begin > pc || point.end <= pc) { return false; }
            owner.sample = pc; return true;
        }
    }
    return false;
}

int main(int argc, char** argv)
{
    if(argc > 2) { return 1; }
    if(argc == 2) { diagnostic_directory = argv[1]; }
    if(::llvm::InitializeNativeTarget() || ::llvm::InitializeNativeTargetAsmPrinter() || ::llvm::InitializeNativeTargetAsmParser()) { return 1; }
    for(unsigned kind: {0x7fu,0x7eu,0x7du,0x7cu,0x7bu})
    {
        bool required{true};
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4 && !defined(__i386__) && !defined(__arm__) && !defined(__powerpc__)
        if(kind == 0x7eu) { required = false; }
#endif
#if !(defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__ALTIVEC__) || defined(__ARM_NEON) || defined(__mips_msa) || defined(__loongarch_sx) || defined(__VEC__))
        if(kind == 0x7bu) { required = false; }
#endif
#if defined(__linux__) && defined(__powerpc__)
        if(kind == 0x7bu) { required = (::getauxval(AT_HWCAP) & 0x10000000ul) != 0u; }
#elif defined(__linux__) && defined(__loongarch__)
        if(kind == 0x7bu) { required = (::getauxval(AT_HWCAP) & (1ul << 4u)) != 0u; }
#endif
        native_owner numeric{}; CHECK(build(numeric, 4u, true, kind, required));
        if(kind == 0x7bu && required)
        {
            // Literal vector operands also need an actual whole-register SSA
            // location; a constant-only debug expression cannot satisfy it.
            native_owner literal{}; CHECK(build(literal, 6u, true, kind, true, true));
            ::fast_io::io::println("whole-register literal vector location required=true hardware-capture-qualified=false");
        }
        ::fast_io::io::println("whole-register location kind=",kind," required=",required," hardware-capture-qualified=false");
    }
#if defined(__linux__) && defined(__mips__) && __SIZEOF_POINTER__ == 8
    native_owner branch_debug{}, branch_ordinary{};
    CHECK(build(branch_debug,7u,true,0x7eu,true,false,true));
    CHECK(build(branch_ordinary,7u,false,0x7eu,false,false,true));
#endif
    native_owner original{}, replacement{}, ordinary{};
    CHECK(build(original, 4u, true));
    CHECK(build(replacement, 5u, true));
    CHECK(build(ordinary, 4u, false));
    CHECK(original.begin != replacement.begin && original.sample != 0u && replacement.sample != 0u);
    auto const old{original.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 7u)};
    auto const fresh{replacement.rows.lookup(replacement.sample, replacement.begin, replacement.end, "uwvm-m2-f3-g5.wasm-native-v1", 3u, 7u)};
    CHECK(old.state == provenance::status::exact && fresh.state == provenance::status::exact);
    CHECK(original.rows.lookup(replacement.sample, replacement.begin, replacement.end, "uwvm-m2-f3-g5.wasm-native-v1", 3u, 7u).state == provenance::status::unavailable);
    CHECK(replacement.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 7u).state == provenance::status::unavailable);
    CHECK(original.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 8u).state == provenance::status::unavailable);
    original.rows.invalidate_runtime_generation();
    CHECK(original.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 7u).state == provenance::status::unavailable);
    CHECK(old.state == provenance::status::exact); // Retained DATA grants no future live authority.
    ::fast_io::io::println("PASS actual MCJIT object callback, pending commit, independent engine/generation maps, ordinary opt-out, epoch revocation; qualified required numeric metadata ranges; physical register capture is a separate product check; no generated function executed");
}

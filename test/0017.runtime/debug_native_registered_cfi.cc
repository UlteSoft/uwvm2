// Real MCJIT + original production memory manager + actual relocated EH CFI.
// The copied-stack evaluator cases below use owned DATA, not an authenticated
// live stack. This test does not qualify physical callers or native finish.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/InlineAsm.h>
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
    ::std::array<extent, 3u> actual{};
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
            else if(*name == "uwvm_cfi_expression") { index = 2u; }
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
    unsigned optimization{};
    CHECK(argc == 1 || argc == 2);
    if(argc == 2)
    {
        auto const end{argv[1] + ::std::strlen(argv[1])};
        auto const parsed{::fast_io::parse_by_scan(argv[1], end, ::fast_io::mnp::dec_get<true, true>(optimization))};
        CHECK(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end);
    }
    CHECK(optimization == 0u || optimization == 3u);
    CHECK(!::llvm::InitializeNativeTarget()); CHECK(!::llvm::InitializeNativeTargetAsmPrinter());
    CHECK(!::llvm::InitializeNativeTargetAsmParser());
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("uwvm_native_registered_cfi", context)};
    auto type{::llvm::FunctionType::get(::llvm::Type::getInt64Ty(context), {::llvm::Type::getInt64Ty(context)}, false)};
    auto leaf{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage, "uwvm_cfi_leaf", *module)};
    auto caller{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage, "uwvm_cfi_caller", *module)};
    auto expression{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage, "uwvm_cfi_expression", *module)};
    for(auto function: {leaf, caller, expression})
    {
        function->setUWTableKind(::llvm::UWTableKind::Async);
        function->addFnAttr(::llvm::Attribute::NoInline);
        function->addFnAttr("frame-pointer", "all");
    }
    ::llvm::IRBuilder<> builder{::llvm::BasicBlock::Create(context, "entry", leaf)};
    builder.CreateRet(builder.CreateAdd(leaf->getArg(0u), builder.getInt64(1u)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context, "entry", caller));
    auto slot{builder.CreateAlloca(builder.getInt64Ty(), builder.getInt32(128u))};
    builder.CreateStore(caller->getArg(0u), slot, true);
    auto value{builder.CreateCall(leaf, {caller->getArg(0u)})};
    builder.CreateRet(builder.CreateAdd(value, builder.CreateLoad(builder.getInt64Ty(), slot, true)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context, "entry", expression));
    // A genuine emitted RA expression equivalent to *(CFA - 8). The ordinary
    // unwinder can handle this valid CFI; our bounded evaluator must refuse it.
    auto assembly{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(), false),
        ".cfi_escape 0x10, 0x10, 0x04, 0x9c, 0x11, 0x78, 0x22\n\tnop", "", true)};
    builder.CreateCall(assembly);
    builder.CreateRet(builder.CreateAdd(expression->getArg(0u), builder.getInt64(1u)));
    CHECK(!::llvm::verifyModule(*module));
    auto manager{::std::make_unique<cfi::runtime_llvm_jit_section_memory_manager>()};
    auto observer{manager.get()};
    symbols loaded{};
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder(::std::move(module))
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(optimization == 3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(manager)).create()}};
    CHECK(engine); engine->RegisterJITEventListener(&loaded); engine->finalizeObject();
    CHECK(!observer->has_finalization_failure() && loaded.valid);
    auto const leaf_address{engine->getFunctionAddress("uwvm_cfi_leaf")};
    auto const caller_address{engine->getFunctionAddress("uwvm_cfi_caller")};
    CHECK(leaf_address == loaded.actual[0u].begin && caller_address == loaded.actual[1u].begin);
    CHECK(reinterpret_cast<::std::uint64_t (*)(::std::uint64_t)>(caller_address)(20u) == 41u);

    auto const expression_address{engine->getFunctionAddress("uwvm_cfi_expression")};
    CHECK(expression_address == loaded.actual[2u].begin);
    CHECK(reinterpret_cast<::std::uint64_t (*)(::std::uint64_t)>(expression_address)(20u) == 21u);
    unsigned checked{}, transitions{}, unsupported{};
    for(auto const function: loaded.actual)
    {
        CHECK(function.begin != 0u && function.end > function.begin);
        cfi::native_debug_cfi_row first{}, previous{};
        CHECK(observer->copy_debug_native_cfi_row(function.begin, function.end, function.begin, first));
        CHECK(first.usable && first.cfa_register == 7u && first.cfa_offset == 8);
        CHECK(first.registers[16u].kind == cfi::native_debug_cfi_rule_kind::cfa_memory && first.registers[16u].offset == -8);
        for(auto pc{function.begin}; pc < function.end; ++pc)
        {
            cfi::native_debug_cfi_row row{};
            if(!observer->copy_debug_native_cfi_row(function.begin, function.end, pc, row))
            {
                CHECK(function.begin == expression_address && !row.usable && row.begin == 0u);
                cfi::native_debug_cfa_row scalar{};
                CHECK(observer->copy_debug_native_cfa_row(function.begin,function.end,pc,scalar) && scalar.usable &&
                      (scalar.cfa_register == 7u || scalar.cfa_register == 6u));
                // A valid CFA must not invent the unsupported expression's RA.
                CHECK(!observer->copy_debug_native_cfi_row(function.begin,function.end,pc,row) && !row.usable);
                ++unsupported; continue;
            }
            CHECK(row.begin <= pc && row.end > pc && row.begin >= function.begin && row.end <= function.end);
            if(row.begin != previous.begin) { ++transitions; }
            previous = row; ++checked;
            CHECK(row.cfa_register == 7u || row.cfa_register == 6u);
            ::std::array<::std::byte, 4096u> owned{};
            ::std::array<::std::uint64_t, 17u> gpr{}; gpr[7u] = 0x1000u; gpr[6u] = 0x1020u;
            auto const cfa_value{gpr[row.cfa_register] + row.cfa_offset};
            CHECK(cfa_value > gpr[7u] && cfa_value <= 0x1000u + owned.size());
            CHECK(row.registers[16u].kind == cfi::native_debug_cfi_rule_kind::cfa_memory && row.registers[16u].offset == -8);
            ::std::uint64_t const expected{0x12345678u};
            ::std::memcpy(owned.data() + (cfa_value - 8u - 0x1000u), &expected, 8uz);
            cfi::native_debug_cfi_caller body{};
            CHECK(cfi::evaluate_native_debug_cfi_x64(row, gpr, (1u << 7u) | (1u << 6u), 0x1000u, owned, body));
            CHECK(body.return_pc == expected && body.cfa == cfa_value);
            CHECK(!(body.known & ~((1u << 7u) | (1u << 16u) | (1u << 6u))));
            CHECK(!cfi::evaluate_native_debug_cfi_x64(row, gpr, (1u << 7u) | (1u << 6u), 0x1000u,
                {owned.data(), static_cast<::std::size_t>(cfa_value - 0x1000u - 1u)}, body));
        }
        cfi::native_debug_cfi_row rejected{first};
        CHECK(!observer->copy_debug_native_cfi_row(function.begin - 1u, function.end, function.begin, rejected));
        CHECK(!rejected.usable && rejected.begin == 0u);
        CHECK(!observer->copy_debug_native_cfi_row(function.begin, function.end + 1u, function.begin, rejected));
        CHECK(!observer->copy_debug_native_cfi_row(function.begin, function.end - 1u, function.begin, rejected));
        CHECK(!observer->copy_debug_native_cfi_row(function.begin, function.end, function.end, rejected));

        // Genuine compiler-derived entry rule; bytes below are an owned test
        // allocation. Their integer label has no native address authority.
        ::std::array<::std::byte, 128u> copy{};
        ::std::uint64_t const expected_pc{0x12345678u};
        ::std::memcpy(copy.data(), &expected_pc, sizeof(expected_pc));
        ::std::array<::std::uint64_t, 17u> registers{}; registers[7u] = 0x1000u;
        cfi::native_debug_cfi_caller recovered{};
        CHECK(cfi::evaluate_native_debug_cfi_x64(first, registers, 1u << 7u, 0x1000u, copy, recovered));
        CHECK(recovered.return_pc == expected_pc && recovered.cfa == 0x1008u);
        CHECK(recovered.known == ((1u << 7u) | (1u << 16u))); // Omitted registers remain unknown.
        CHECK(!cfi::evaluate_native_debug_cfi_x64(first, registers, 0u, 0x1000u, copy, recovered));
        CHECK(recovered.known == 0u && recovered.return_pc == 0u);
        CHECK(!cfi::evaluate_native_debug_cfi_x64(first, registers, 1u << 7u, 0x1000u, {copy.data(), 7uz}, recovered));
        CHECK(!cfi::evaluate_native_debug_cfi_x64(first, registers, 1u << 7u,
            (::std::numeric_limits<::std::uintptr_t>::max)() - 4u, copy, recovered));
        registers[7u] = 0x0fffu;
        CHECK(!cfi::evaluate_native_debug_cfi_x64(first, registers, 1u << 7u, 0x1000u, copy, recovered));
    }

    // Owned DATA only: a genuine live caller still needs the separate runtime
    // trap/owner/CFI/worker-stack transaction. Sparse storage is constant size
    // even when the CFA lies beyond the old 64KiB scratch limit.
    {
        cfi::native_debug_cfi_row row{}; row.usable = true;
        row.cfa_register = 7u; row.cfa_offset = 200000;
        row.registers[16u] = {cfi::native_debug_cfi_rule_kind::cfa_memory, 0u, -8};
        row.registers[3u] = {cfi::native_debug_cfi_rule_kind::cfa_memory, 0u, -16};
        row.registers[12u] = {cfi::native_debug_cfi_rule_kind::cfa_memory, 0u, -24};
        ::std::array<::std::uint64_t, 17u> registers{}; registers[7u] = 0x1000u;
        auto const end{static_cast<::std::uintptr_t>(0x1000u + 200000u)};
        ::std::array<cfi::native_debug_cfi_owned_word, 3u> words{{
            {end - 8u, 0x12345678u}, {end - 16u, 42u}, {}}};
        cfi::native_debug_cfi_caller recovered{};
        CHECK(cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            0x1000u, end, {words.data(), 2u}, recovered));
        CHECK(recovered.cfa == end && recovered.return_pc == 0x12345678u &&
            recovered.registers[3u] == 42u && !(recovered.known & (1u << 12u)));
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            0x1000u, end, {words.data() + 1u, 1u}, recovered));
        CHECK(!recovered.known && !recovered.return_pc);
        words[2u] = words[0u];
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            0x1000u, end, words, recovered)); // Duplicate evidence refuses.
        words[2u] = {end - 7u, 0u};
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            0x1000u, end, words, recovered));
        words[2u] = {0x0fffu, 0u};
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            0x1000u, end, words, recovered));
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 0u,
            0x1000u, end, {words.data(), 2u}, recovered));
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            0x1000u, end - 1u, {words.data(), 2u}, recovered));
        registers[7u] = UINTPTR_MAX - 4u;
        CHECK(!cfi::evaluate_native_debug_cfi_x64_sparse(row, registers, 1u << 7u,
            UINTPTR_MAX - 4u, UINTPTR_MAX, {}, recovered));
    }

    CHECK(checked > 0u && transitions >= 6u && unsupported > 0u);
    observer->deregisterEHFrames();
    for(auto const function: loaded.actual)
    {
        cfi::native_debug_cfi_row row{};
        CHECK(!observer->copy_debug_native_cfi_row(function.begin, function.end, function.begin, row));
        CHECK(!row.usable && row.begin == 0u);
        cfi::native_debug_cfa_row scalar{};
        CHECK(!observer->copy_debug_native_cfa_row(function.begin,function.end,function.begin,scalar) && !scalar.usable);
    }
    engine->UnregisterJITEventListener(&loaded);
    engine.reset();
    ::fast_io::io::println("actual registered CFI passed optimization=", optimization, " code-bytes=", checked, " rule-transitions=", transitions,
        " unsupported-expression-bytes=", unsupported, " owned-data-evaluator=true physical-caller-qualified=false native-finish-qualified=false");
}

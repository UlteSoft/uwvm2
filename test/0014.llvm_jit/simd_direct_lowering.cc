// Differential test of every supported SIMD opcode. With arguments TRIPLE CPU FEATURES PREFIX,
// also writes target IR and a portable C harness for cross-codegen/QEMU checks in a temporary directory.
// Append --ir-only for codegen inventories that do not execute the C oracle.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace s = uwvm2::runtime::compiler::shared;
namespace v = s::wasm1p1_simd_details;
using code = d::llvm_jit_simd_code;
using kind = s::wasm1p1_simd_instruction_kind;
using scalar = s::wasm1p1_simd_scalar_kind;
using bytes = std::array<std::byte, 64>;
using oracle = void (*)(bytes&, bytes const&, unsigned);

template <typename T> T read(std::byte const* p)
{
    std::array<std::byte, sizeof(T)> raw;
    std::memcpy(raw.data(), p, sizeof(T));
    if constexpr(std::is_arithmetic_v<T> && std::endian::native == std::endian::big)
    { for(std::size_t i{}; i < sizeof(T) / 2; ++i) { std::swap(raw[i], raw[sizeof(T) - i - 1]); } }
    return std::bit_cast<T>(raw);
}
template <typename T> void write(std::byte* p, T value)
{
    auto raw{std::bit_cast<std::array<std::byte, sizeof(T)>>(value)};
    if constexpr(std::is_arithmetic_v<T> && std::endian::native == std::endian::big)
    { for(std::size_t i{}; i < sizeof(T) / 2; ++i) { std::swap(raw[i], raw[sizeof(T) - i - 1]); } }
    std::memcpy(p, raw.data(), sizeof(T));
}

// Only numeric FP operations may choose a different arithmetic NaN. abs/neg/pmin/pmax, lane operations,
// loads and stores are compared bit-for-bit, including signaling NaNs and the sign of zero.
unsigned nan_width(code op)
{
    switch(op)
    {
#define FP_NANS(P, N) \
        case code::P##_sqrt: case code::P##_ceil: case code::P##_floor: case code::P##_trunc: \
        case code::P##_nearest: case code::P##_add: case code::P##_sub: case code::P##_mul: \
        case code::P##_div: case code::P##_min: case code::P##_max: return N;
        FP_NANS(f32x4, 32u)
        FP_NANS(f64x2, 64u)
#undef FP_NANS
        case code::f32x4_demote_f64x2_zero: return 32u;
        case code::f64x2_promote_low_f32x4: return 64u;
        default: return 0u;
    }
}
void canonicalize(bytes& out, unsigned bits, bool canonical_only = false)
{
    if(bits == 32u)
    {
        for(unsigned i{}; i != 16u; i += 4u)
        {
            auto x{read<std::uint32_t>(out.data() + i)};
            if((x & 0x7fc00000u) == 0x7fc00000u) { write(out.data() + i, canonical_only ? x & 0x7fffffffu : std::uint32_t{0x7fc00000u}); }
        }
    }
    if(bits == 64u)
    {
        for(unsigned i{}; i != 16u; i += 8u)
        {
            auto x{read<std::uint64_t>(out.data() + i)};
            if((x & 0x7ff8000000000000ull) == 0x7ff8000000000000ull)
            { write(out.data() + i, static_cast<std::uint64_t>(canonical_only ? x & 0x7fffffffffffffffull : 0x7ff8000000000000ull)); }
        }
    }
}

constexpr unsigned char shuffle_lanes[16]{31, 0, 17, 14, 29, 2, 19, 12, 27, 4, 21, 10, 25, 6, 23, 8};
template <code Op, kind Kind, scalar Scalar>
void reference(bytes& out, bytes const& in, unsigned lane)
{
    auto a{read<v::wasm_v128>(in.data())};
    auto b{read<v::wasm_v128>(in.data() + 16)};
    auto c{read<v::wasm_v128>(in.data() + 32)};
    if constexpr(Kind == kind::constant) { write(out.data(), read<v::wasm_v128>(reinterpret_cast<std::byte const*>(shuffle_lanes))); }
    else if constexpr(Kind == kind::shuffle) { write(out.data(), v::eval_shuffle(a, b, v::make_shuffle_controls(shuffle_lanes))); }
    else if constexpr(Kind == kind::unary) { write(out.data(), v::eval_full_unop<Op>(a)); }
    else if constexpr(Kind == kind::binary) { write(out.data(), v::eval_full_binop<Op>(a, b)); }
    else if constexpr(Kind == kind::ternary) { write(out.data(), v::eval_bitselect(a, b, c)); }
    else if constexpr(Kind == kind::shift) { write(out.data(), v::eval_full_shift<Op>(a, read<v::wasm_i32>(in.data() + 48))); }
    else if constexpr(Kind == kind::test) { write(out.data(), v::eval_full_test<Op>(a)); }
    else if constexpr(Kind == kind::memory_load) { write(out.data(), v::eval_memory_load<Op>(in.data() + 1, a, lane)); }
    else if constexpr(Kind == kind::memory_store) { v::eval_memory_store<Op>(out.data() + 3, a, lane); }
#define SCALAR_REFERENCE(K, T) \
    else if constexpr(Scalar == scalar::K) \
    { \
        auto x{read<v::wasm_##T>(in.data() + 48)}; \
        if constexpr(Kind == kind::splat) { write(out.data(), v::eval_full_splat_##T<Op>(x)); } \
        else if constexpr(Kind == kind::extract_lane) { write(out.data(), v::eval_extract_lane_##T<Op>(a, lane)); } \
        else if constexpr(Kind == kind::replace_lane) { write(out.data(), v::eval_replace_lane_##T<Op>(a, x, lane)); } \
    }
    SCALAR_REFERENCE(i32, i32)
    SCALAR_REFERENCE(i64, i64)
    SCALAR_REFERENCE(f32, f32)
    SCALAR_REFERENCE(f64, f64)
#undef SCALAR_REFERENCE
    canonicalize(out, nan_width(Op));
}

struct entry { llvm::Function* fn; oracle expected; unsigned op; unsigned lane; };

template <code Op, kind Kind, scalar Scalar>
llvm::Function* emit(llvm::Module& m, unsigned lane, std::string const& cpu, std::string const& features)
{
    auto& ctx{m.getContext()};
    llvm::IRBuilder<> b{ctx};
    auto ptr{llvm::PointerType::getUnqual(ctx)};
    auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), {ptr, ptr}, false),
                                  llvm::Function::ExternalLinkage, "simd_" + std::to_string(unsigned(Op)) + "_" + std::to_string(lane), m)};
    fn->addFnAttr("target-features", features);
    fn->addFnAttr("target-cpu", cpu);
    b.SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", fn));
    d::simd_ir::emitter e{b};
    auto load{[&](llvm::Type* type, unsigned offset) -> llvm::Value*
    {
        auto value{b.CreateLoad(type, b.CreateGEP(b.getInt8Ty(), fn->getArg(1), b.getInt32(offset)))};
        value->setAlignment(llvm::Align{1});
        return value;
    }};
    auto a{load(e.integer(8u), 0)};
    auto c{load(e.integer(8u), 16)};
    auto z{load(e.integer(8u), 32)};
    llvm::Value* result{};
    if constexpr(Kind == kind::memory_load)
    { result = d::simd_ir::emit_load<Op>(b, b.CreateGEP(b.getInt8Ty(), fn->getArg(1), b.getInt32(1)), a, lane); }
    else if constexpr(Kind == kind::memory_store)
    {
        if(!d::simd_ir::emit_store<Op>(b, b.CreateGEP(b.getInt8Ty(), fn->getArg(0), b.getInt32(3)), a, lane)) { std::abort(); }
    }
    else
    {
        if constexpr(Kind == kind::shift) { c = e.endian(load(b.getInt32Ty(), 48)); }
        else if constexpr(Kind == kind::splat || Kind == kind::replace_lane)
        {
            constexpr bool wide{Scalar == scalar::i64 || Scalar == scalar::f64};
            auto x{e.endian(load(b.getIntNTy(wide ? 64 : 32), 48))};
            if constexpr(Scalar == scalar::f32) { x = b.CreateBitCast(x, b.getFloatTy()); }
            if constexpr(Scalar == scalar::f64) { x = b.CreateBitCast(x, b.getDoubleTy()); }
            if constexpr(Kind == kind::splat) { a = x; } else { c = x; }
        }
        result = d::simd_ir::emit_value(b, Op, a, c, z, lane, reinterpret_cast<std::byte const*>(shuffle_lanes));
    }
    if constexpr(Kind != kind::memory_store)
    {
        if(!result) { std::fprintf(stderr, "missing SIMD opcode %u\n", unsigned(Op)); std::abort(); }
        if(result->getType()->isFloatingPointTy()) { result = b.CreateBitCast(result, b.getIntNTy(result->getType()->getScalarSizeInBits())); }
        // Scalar results have canonical little-endian bytes too. A v128 byte vector is already in memory order.
        if(!result->getType()->isVectorTy()) { result = e.endian(result); }
        b.CreateStore(result, fn->getArg(0))->setAlignment(llvm::Align{1});
    }
    b.CreateRetVoid();
    for(auto& block: *fn) for(auto& ins: block)
    {
        if(auto call{llvm::dyn_cast<llvm::CallInst>(&ins)})
        {
            if(call->getCalledFunction() && call->getCalledFunction()->isIntrinsic()) { continue; }
            // A CallInst containing one opaque native instruction is not a host bridge.
            // Whitelist the LoongArch scalar instructions used when LSX is unavailable.
            if(auto assembly{llvm::dyn_cast<llvm::InlineAsm>(call->getCalledOperand())};
               assembly && e.target().isLoongArch() && !assembly->hasSideEffects())
            {
                bool native{};
                for(char const* instruction : {"fadd.s $0, $1, $2", "fsub.s $0, $1, $2", "fmul.s $0, $1, $2", "fdiv.s $0, $1, $2",
                                                "fadd.d $0, $1, $2", "fsub.d $0, $1, $2", "fmul.d $0, $1, $2", "fdiv.d $0, $1, $2",
                                                "fcvt.s.d $0, $1", "fcvt.d.s $0, $1"})
                { native |= assembly->getAsmString() == instruction; }
                if(native) { continue; }
            }
            std::fprintf(stderr, "unexpected helper for SIMD opcode %u\n", unsigned(Op));
            std::abort(); // No UWVM numeric helper or per-access bridge is allowed.
        }
    }
    return fn;
}

int main(int argc, char** argv)
{
    bool const ir_only{argc == 6 && std::strcmp(argv[5], "--ir-only") == 0};
    if(argc != 1 && argc != 5 && !ir_only) { return 2; }
    bool const cross{argc == 5 || ir_only};
    // Exercise the actual production final-legalization helper, not just an equivalent opt command.
    {
        llvm::LLVMContext ctx;
        llvm::Module scalarized{"sparc-final-legalization", ctx};
#if LLVM_VERSION_MAJOR >= 21
        scalarized.setTargetTriple(llvm::Triple{"sparc-linux-gnu"});
#else
        scalarized.setTargetTriple("sparc-linux-gnu");
#endif
        scalarized.setDataLayout("E-p:32:32");
        llvm::IRBuilder<> b{ctx};
        auto narrow{llvm::FixedVectorType::get(b.getInt8Ty(), 8)};
        auto wide{llvm::FixedVectorType::get(b.getInt16Ty(), 8)};
        auto fn{llvm::Function::Create(llvm::FunctionType::get(wide, {narrow}, false),
            llvm::Function::ExternalLinkage, "extend", scalarized)};
        b.SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", fn));
        b.CreateRet(b.CreateSExt(fn->getArg(0), wide));
        d::legalize_llvm_jit_native_vectors(scalarized);
        unsigned scalar_extensions{};
        for(auto& block: *fn) for(auto& instruction: block)
        {
            if(llvm::isa<llvm::SExtInst>(instruction))
            {
                if(instruction.getType()->isVectorTy()) { return 4; }
                ++scalar_extensions;
            }
        }
        if(scalar_extensions != 8u || llvm::verifyModule(scalarized, &llvm::errs())) { return 4; }
    }
#if defined(UWVM2TEST_SIMD_CROSS_TARGETS)
    llvm::InitializeAllTargetInfos(); llvm::InitializeAllTargets(); llvm::InitializeAllTargetMCs(); llvm::InitializeAllAsmPrinters();
#else
    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
#endif
    llvm::InitializeNativeTargetAsmParser();
    // LLVM Triple's constructor is not a command-line triple normalizer.
    // In particular mips64-linux-gnuabin32 must acquire an explicit vendor
    // before target selection; otherwise emission may use N64 pointer layout
    // while a later llc -target-abi=n32 silently uses a different contract.
    auto triple{llvm::Triple::normalize(cross ? std::string{argv[1]} : llvm::sys::getDefaultTargetTriple())};
    auto cpu{cross ? std::string{argv[2]} : llvm::sys::getHostCPUName().str()};
    auto features{cross ? std::string{argv[3]} : std::string{}};
    if(!cross) for(auto const& [name, enabled]: llvm::sys::getHostCPUFeatures())
    { features += enabled ? '+' : '-'; features += name; features += ','; }
    if(!features.empty() && features.back() == ',') { features.pop_back(); }
    std::string error;
    // The two-argument overload takes Triple rather than text in LLVM 23.1.
    // This overload exists in older LLVM too; an empty architecture name asks
    // the registry to use the explicitly parsed destination triple unchanged.
    llvm::Triple lookup_triple{triple};
    auto target{llvm::TargetRegistry::lookupTarget("", lookup_triple, error)};
    if(!target) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
#if LLVM_VERSION_MAJOR >= 21
    auto target_triple{llvm::Triple{triple}};
#else
    auto target_triple{triple};
#endif
    std::unique_ptr<llvm::TargetMachine> machine{target->createTargetMachine(target_triple, cpu, features, {}, llvm::Reloc::PIC_)};
    llvm::LLVMContext context;
    auto module{std::make_unique<llvm::Module>("simd-direct-differential", context)};
    module->setTargetTriple(target_triple); module->setDataLayout(machine->createDataLayout());
    if(lookup_triple.isABIN32() && module->getDataLayout().getPointerSizeInBits() != 32u) { return 3; }
    // Also export the production store preflight for the signal-based cross-page no-partial-write test.
    for(unsigned width: {1u, 2u, 4u, 8u, 16u, 32u, 64u})
    {
        llvm::IRBuilder<> b{context};
        auto ptr{llvm::PointerType::getUnqual(context)};
        auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), {ptr, b.getInt32Ty(), ptr}, false),
            llvm::Function::ExternalLinkage, "guarded_store_" + std::to_string(width), *module)};
        fn->addFnAttr("target-cpu", cpu); fn->addFnAttr("target-features", features);
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
        auto offset{b.CreateZExt(fn->getArg(1), b.getInt64Ty())};
        auto pointer{b.CreateGEP(b.getInt8Ty(), fn->getArg(0), offset)};
        d::emit_llvm_jit_guarded_store_preflight(b, pointer, offset, width, 16u);
        auto type{llvm::FixedVectorType::get(b.getInt8Ty(), width)};
        auto value{b.CreateLoad(type, fn->getArg(2))}; value->setAlignment(llvm::Align{1});
        static_cast<void>(d::finalize_llvm_jit_direct_memory_store(b.CreateStore(value, pointer), llvm::Align{1}));
        b.CreateRetVoid();
    }
    std::vector<entry> entries;
    unsigned count{};
    for(unsigned op{}; op != 256; ++op)
    {
        bool found{s::visit_wasm1p1_simd_instruction(static_cast<code>(op),
            [&]<code Op, kind Kind, scalar Scalar, std::size_t Lanes, std::uint_least32_t Align>()
            {
                for(unsigned lane{}; lane != (Lanes ? Lanes : 1); ++lane)
                { entries.push_back({emit<Op, Kind, Scalar>(*module, lane, cpu, features), reference<Op, Kind, Scalar>, op, lane}); }
                return true;
            })};
        count += found;
    }
    if(count != 236 || llvm::verifyModule(*module, &llvm::errs())) { return 2; }
    std::vector<bytes> inputs(256);
    std::uint64_t rng{0x778899aabbccddull};
    constexpr std::uint32_t special[]{0, 1, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 0xffffffff, 0x80000000,
        0x7fffffff, 0x80008000, 0x7fff7fff, 0xff80ff7f, 0x3f000000, 0x3fc00000, 0x40200000, 0xbf000000,
        0xbfc00000, 0xc0200000, 0x7f800000, 0xff800000, 0x7fc12345, 0x7fa12345, 0x00800000, 0x007fffff,
        0x4f000000, 0x4f800000, 0xcf000000, 0xcf800000, 0x3ff00000, 0x7ff00000, 0x7ff81234, 0xfff00000};
    constexpr std::uint64_t special64[]{0, 1, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
        0x7ff8123456789abcull, 0x7ff0123456789abcull, 0xfff8123456789abcull, 0xffffffffffffffffull,
        0x0010000000000000ull, 0x000fffffffffffffull, 0x3fe0000000000000ull, 0x3ff8000000000000ull,
        0x4004000000000000ull, 0xbfe0000000000000ull, 0xbff8000000000000ull, 0xc004000000000000ull,
        0x41e0000000000000ull, 0x41efffffffe00000ull, 0x41f0000000000000ull, 0xc1e0000000000000ull};
    for(unsigned i{}; i != inputs.size(); ++i)
    {
        for(auto& byte: inputs[i]) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; byte = std::byte(rng & 255); }
        if(i < 128) for(unsigned j{}; j != 16; ++j)
        { write(inputs[i].data() + j * 4, special[(i + (i >= 64 ? j : 0)) % std::size(special)]); }
        if(i >= 128 && i < 160) for(unsigned j{}; j != 8; ++j)
        { write(inputs[i].data() + j * 8, special64[(i - 128 + j) % std::size(special64)]); }
        if(i >= 160 && i < 176) for(unsigned j{}; j != 16; ++j)
        { inputs[i][16 + j] = std::byte((i - 160) * 16 + j); } // Every possible byte index, including 16..127 and 128..255.
        if(i == 176 || i == 178) for(unsigned j{}; j != 16; ++j)
        { write(inputs[i].data() + j * 4, std::uint32_t{i == 176 ? 0x7fc00000u : 0xffc00000u}); }
        if(i == 177 || i == 179) for(unsigned j{}; j != 8; ++j)
        { write(inputs[i].data() + j * 8, std::uint64_t{i == 177 ? 0x7ff8000000000000ull : 0xfff8000000000000ull}); }
    }
    if(cross)
    {
        std::error_code ec;
        llvm::raw_fd_ostream ir{std::string{argv[4]} + ".ll", ec};
        if(ec) { return 3; } module->print(ir, nullptr);
        ir.flush();
        if(ir.has_error()) { return 3; }
        // The registered-target inventory consumes IR only. Repeating the
        // 256-case C oracle for every target wasted >1.5 GiB per sweep; keep
        // the full oracle as the default for actual cross execution tests.
        // Argument validation above prevents a typo from silently running a
        // native test instead of the requested cross-emission path.
        if(ir_only) { return 0; }
        std::ofstream c{std::string{argv[4]} + ".c"};
        c << "extern int printf(const char*,...);extern int puts(const char*);"
             "extern void* memset(void*,int,__SIZE_TYPE__);extern int memcmp(const void*,const void*,__SIZE_TYPE__);\n";
        for(auto const& ent: entries) { c << "extern void " << ent.fn->getName().str() << "(unsigned char*,const unsigned char*);\n"; }
        c << "static void (*const functions[])(unsigned char*,const unsigned char*)={";
        for(auto const& ent: entries) { c << ent.fn->getName().str() << ','; } c << "};\n";
        c << "static const unsigned nan_bits[]={";
        for(auto const& ent: entries) { c << nan_width(static_cast<code>(ent.op)) << ','; } c << "};\n";
        c << "static const unsigned source64[]={";
        for(auto const& ent: entries)
        {
            auto op{static_cast<code>(ent.op)};
            c << ((nan_width(op) == 64u && op != code::f64x2_promote_low_f32x4) || op == code::f32x4_demote_f64x2_zero) << ',';
        }
        c << "};\n";
        // Normalize arithmetic NaNs in the separately compiled integer-only comparator, NOT in LLVM FP IR.
        // LLVM may legally fold a floating-point NaN select into a sqrt/min that chooses another NaN payload.
        c << "static void canon(unsigned char*p,unsigned bits,int strict){unsigned n=bits/8;if(!n)return;for(unsigned i=0;i<16;i+=n){"
             "unsigned long long x=0;for(unsigned j=0;j<n;++j)x|=(unsigned long long)p[i+j]<<(8*j);"
             "if((x&(n==4?0x7fc00000ULL:0x7ff8000000000000ULL))==(n==4?0x7fc00000ULL:0x7ff8000000000000ULL)){"
             "x=strict?(x&(n==4?0x7fffffffULL:0x7fffffffffffffffULL)):(n==4?0x7fc00000ULL:0x7ff8000000000000ULL);"
             "for(unsigned j=0;j<n;++j)p[i+j]=(unsigned char)(x>>(8*j));}}}\n";
        auto row{[&](bytes const& data)
        { c << '{'; for(auto x: data) { c << std::to_integer<unsigned>(x) << ','; } c << "},\n"; }};
        c << "static const unsigned char inputs[][64]={\n"; for(auto const& in: inputs) { row(in); } c << "};\n";
        c << "static const unsigned char expected[][64]={\n";
        for(auto const& ent: entries) for(auto const& in: inputs)
        { bytes out; out.fill(std::byte{0xa5}); ent.expected(out, in, ent.lane); row(out); }
        c << "};\nint main(void){unsigned char out[64]; unsigned k=0; for(unsigned f=0;f<sizeof(functions)/sizeof(*functions);++f)"
             "for(unsigned i=0;i<256;++i,++k){memset(out,165,64);functions[f](out,inputs[i]);"
             "canon(out,nan_bits[f],i==176||i==178||((i==177||i==179)&&source64[f]));if(memcmp(out,expected[k],64))"
             "{printf(\"FAIL function=%u sample=%u\\n\",f,i);for(unsigned j=0;j<32;++j)printf(\"%02x\",expected[k][j]);puts(\"\");"
             "for(unsigned j=0;j<32;++j)printf(\"%02x\",out[j]);puts(\"\");return 1;}}puts(\"PASS SIMD differential: all 236 opcodes\");return 0;}\n";
        return c ? 0 : 4;
    }
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
        .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).setMCPU(cpu).create()};
    if(!engine) { std::fprintf(stderr, "%s\n", error.c_str()); return 5; }
    engine->finalizeObject();
    for(auto const& ent: entries)
    {
        auto fn{reinterpret_cast<void (*)(std::byte*, std::byte const*)>(engine->getFunctionAddress(ent.fn->getName().str()))};
        for(unsigned i{}; i != inputs.size(); ++i)
        {
            bytes expected, actual; expected.fill(std::byte{0xa5}); actual = expected;
            ent.expected(expected, inputs[i], ent.lane); fn(actual.data(), inputs[i].data());
            auto op{static_cast<code>(ent.op)};
            canonicalize(actual, nan_width(op), i == 176 || i == 178 ||
                ((i == 177 || i == 179) && ((nan_width(op) == 64u && op != code::f64x2_promote_low_f32x4) || op == code::f32x4_demote_f64x2_zero)));
            if(actual != expected)
            {
                std::fprintf(stderr, "FAIL op=%u lane=%u sample=%u\n", ent.op, ent.lane, i);
                for(auto x: expected) { std::fprintf(stderr, "%02x", std::to_integer<unsigned>(x)); } std::fputc('\n', stderr);
                for(auto x: actual) { std::fprintf(stderr, "%02x", std::to_integer<unsigned>(x)); } std::fputc('\n', stderr);
                return 6;
            }
        }
    }
    std::printf("PASS SIMD differential: %u opcodes, %zu lane variants, %zu cases\n", count, entries.size(), entries.size() * inputs.size());
}
#include <uwvm2/utils/macro/pop_macros.h>

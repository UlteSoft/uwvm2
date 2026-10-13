// Actual target MC on owned DATA. This does not mint a VM/code/step lease.
#include <uwvm2/uwvm/debugger/native_wasm_step_boundary.h>
#include <fast_io.h>
#include <array>
#include <span>
#include <cstdint>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace nd = dbg::native_disassembly;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native forward MC failure line=",__LINE__); ::fast_io::fast_terminate(); } } while(false)
struct target_data
{
    bool available{true},little_endian{true};
    unsigned description_version{1u},pointer_bits{sizeof(::std::uintptr_t)*8u};
    unsigned maximum_instruction_bytes{},minimum_instruction_alignment{};
    ::std::array<char,64u> triple{},cpu{},features{};
    ::std::size_t triple_size{},cpu_size{},features_size{};
};
template<::std::size_t N>
static void text(::std::array<char,64u>& out,::std::size_t& size,char const (&literal)[N])
{
    static_assert(N<=64u);
    for(::std::size_t i{};i<N;++i) { out[i]=literal[i]; }
    size=N-1u;
}
struct counted_mc
{
    nd::decoder actual;
    ::std::size_t calls{};
    explicit counted_mc(target_data const& d) : actual{d} {}
    [[nodiscard]] explicit operator bool() const noexcept { return bool(actual); }
    nd::instruction decode(::std::uintptr_t pc,::std::span<::std::uint8_t const> bytes) noexcept
    { ++calls;return actual.decode(pc,bytes); }
};
int main()
{
#if defined(UWVM_USE_LLVM_JIT) && defined(__linux__) && \
    (defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen==64))
    target_data target{};
# if defined(__x86_64__) || defined(__i386__)
#  if defined(__x86_64__)
    text(target.triple,target.triple_size,"x86_64-pc-linux-gnu");
#  else
    text(target.triple,target.triple_size,"i386-pc-linux-gnu");
#  endif
    text(target.cpu,target.cpu_size,"generic");
    target.maximum_instruction_bytes=15u;target.minimum_instruction_alignment=1u;
    ::std::array<::std::uint8_t,5u> code{0x48u,0x83u,0xc0u,0x01u,0x90u};
#  if defined(__i386__)
    code={0x66u,0x83u,0xc0u,0x01u,0x90u};
#  endif
    constexpr unsigned second_size{1u};
# elif defined(__aarch64__)
    text(target.triple,target.triple_size,"aarch64-linux-gnu");
    text(target.cpu,target.cpu_size,"generic");text(target.features,target.features_size,"+neon");
    target.maximum_instruction_bytes=4u;target.minimum_instruction_alignment=4u;
    ::std::array<::std::uint8_t,8u> code{0x1fu,0x20u,0x03u,0xd5u,0x1fu,0x20u,0x03u,0xd5u};
    constexpr unsigned second_size{4u};
# else
    text(target.triple,target.triple_size,"riscv64-linux-gnu");
    text(target.cpu,target.cpu_size,"generic-rv64");text(target.features,target.features_size,"+m,+a,+f,+d,+c");
    // MCAsmInfo describes byte alignment; decoder independently rejects odd RISC-V PCs.
    target.maximum_instruction_bytes=4u;target.minimum_instruction_alignment=1u;
    ::std::array<::std::uint8_t,6u> code{0x93u,0x82u,0x12u,0x00u,0x01u,0x00u};
    constexpr unsigned second_size{2u};
# endif

    // Derive description fields from this linked target's real MC tables.
    // MC assembler alignment can differ from architectural instruction alignment.
    CHECK(nd::details::initialize_native());
    ::llvm::Triple const triple{::llvm::StringRef{target.triple.data(),target.triple_size}};
    ::std::string error{};
    auto const* provider{dbg::native_owned_instruction_semantics::details::lookup_target(triple,error)};
    CHECK(provider!=nullptr);
    auto registers{dbg::native_owned_instruction_semantics::details::make_registers(*provider,triple)};
    CHECK(registers!=nullptr);
    ::llvm::MCTargetOptions options{};
    auto assembly{dbg::native_owned_instruction_semantics::details::make_assembly(*provider,*registers,triple,options)};
    auto subtarget{dbg::native_owned_instruction_semantics::details::make_subtarget(*provider,triple,
        {target.cpu.data(),target.cpu_size},{target.features.data(),target.features_size})};
    CHECK(assembly && subtarget);
    target.little_endian=assembly->isLittleEndian();
    target.minimum_instruction_alignment=assembly->getMinInstAlignment();
    target.maximum_instruction_bytes=dbg::native_owned_instruction_semantics::details::maximum_instruction_length(*assembly,*subtarget,triple);
    counted_mc display{target};dbg::native_owned_instruction_semantics::decoder semantics{target};
    CHECK(display && semantics);
    constexpr ::std::uintptr_t begin{0x1000u};
    auto const forward{nd::decode_window_with(display,code,begin,begin+4u,0,0,3u)};
    CHECK(forward.available && forward.instructions[0u].pc==begin+4u && forward.instructions[0u].size==second_size &&
        !forward.instructions[1u] && !forward.instructions[2u] && display.calls==2u);
    display.calls=0u;
    auto const step{dbg::native_wasm_step_boundary::prepare(display,semantics,code,begin,begin+code.size(),begin,false)};
    CHECK(step && step.first_successor==begin+4u && step.second_successor==0u && display.calls==2u);
    auto const backwards{nd::decode_window_with(display,code,begin,begin+4u,0,-1,3u)};
    CHECK(backwards.available && backwards.instructions[0u].pc==begin && backwards.instructions[0u].size==4u &&
        backwards.instructions[1u].pc==begin+4u && !backwards.instructions[2u]);
    CHECK(!nd::decode_window_with(display,code,begin,begin+1u,0,0,1u).available);
    CHECK(!nd::decode_window_with(display,code,begin,begin+4u,-1,0,1u).available);
    CHECK(!dbg::native_wasm_step_boundary::prepare(display,semantics,code,begin,begin+code.size(),begin+4u,false));
    ::fast_io::io::println("native forward MC: PASS actual target variable/fixed-length boundaries, one entry walk, ordinary successor and owner-end refusal pointer-bits=",target.pointer_bits);
#else
    ::fast_io::io::println("native forward MC: UNAVAILABLE actual Linux target SDK required");return 77;
#endif
}

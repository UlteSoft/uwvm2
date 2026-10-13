// Cold real MC constructor allocation-failure regression; DATA only.
// No kernel trap, native PC dereference, execution/call/owner permission test.
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <fast_io.h>
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
# error This allocation-unwind fixture requires real C++ exceptions; never count SKIP as PASS.
#endif
namespace dbg = ::uwvm2::uwvm::debugger;
static ::std::atomic_bool fail_context_sized_new{};
static ::std::atomic_uint failures{};
void* operator new(::std::size_t bytes)
{
    if(bytes == sizeof(::llvm::MCContext) && fail_context_sized_new.exchange(false, ::std::memory_order_acq_rel))
    { failures.fetch_add(1u, ::std::memory_order_relaxed); throw ::std::bad_alloc{}; }
    return ::fast_io::native_global_allocator::allocate(bytes);
}
void operator delete(void* allocation) noexcept { ::fast_io::native_global_allocator::deallocate(allocation); }
void operator delete(void* allocation, ::std::size_t) noexcept { ::fast_io::native_global_allocator::deallocate(allocation); }
static void require(bool value, char const* message) noexcept
{
    if(!value) { ::fast_io::io::perrln("native MC allocation: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
// OWNED structural MC target DATA only; it does not purport to be a published
// execution engine. Actual target tables below supply max/alignment/endianness.
struct description
{
    ::std::array<char,128u> triple{}, cpu{}, features{};
    ::std::size_t triple_size{}, cpu_size{}, features_size{};
    unsigned description_version{1u}, pointer_bits{sizeof(::std::uintptr_t)*8u};
    ::std::size_t maximum_instruction_bytes{}, minimum_instruction_alignment{};
    bool little_endian{}, available{true};
};
int main()
{
    static_assert(::std::is_nothrow_default_constructible_v<dbg::native_owned_instruction_semantics::decoder>);
    static_assert(::std::is_nothrow_constructible_v<dbg::native_owned_instruction_semantics::decoder, description const&>);
    dbg::native_owned_instruction_semantics::decoder warm{};
    require(bool(warm), "real host LLVM MC provider required; missing closure is FAIL");
    description target{};
    ::llvm::Triple triple{dbg::native_owned_instruction_semantics::details::native_triple};
    auto const text{triple.str()}; require(text.size() < target.triple.size(), "owned triple bound");
    for(::std::size_t index{}; index != text.size(); ++index)
    {
        // [owned Triple string ... size][local description array ... capacity]
        // [safe] both extents checked BEFORE each byte copy; no native address.
        target.triple[index] = text[index];
    }
    target.triple_size = text.size(); target.triple[target.triple_size] = '\0';
    ::std::string error{};
    auto const* actual{dbg::native_owned_instruction_semantics::details::lookup_target(triple,error)};
    require(actual != nullptr, "actual registered host target");
    auto registers{dbg::native_owned_instruction_semantics::details::make_registers(*actual,triple)};
    require(bool(registers), "actual MC register table");
    auto assembly{dbg::native_owned_instruction_semantics::details::make_assembly(*actual,*registers,triple)};
    auto subtarget{dbg::native_owned_instruction_semantics::details::make_subtarget(*actual,triple)};
    require(assembly && subtarget, "actual target assembly/subtarget");
    target.maximum_instruction_bytes = dbg::native_owned_instruction_semantics::details::maximum_instruction_length(*assembly,*subtarget);
    target.minimum_instruction_alignment = assembly->getMinInstAlignment(); target.little_endian = assembly->isLittleEndian();
    require(dbg::native_target_metadata::valid(target), "actual tables produce bounded structural DATA");
    {
        fail_context_sized_new.store(true, ::std::memory_order_release);
        dbg::native_owned_instruction_semantics::decoder failed{};
        auto const still_armed{fail_context_sized_new.exchange(false, ::std::memory_order_acq_rel)};
        require(!still_armed && failures.load(::std::memory_order_relaxed) == 1u && !failed,
            "default constructor catches one genuine context-sized C++ allocation failure and supplies no provider");
    }
    {
        fail_context_sized_new.store(true, ::std::memory_order_release);
        dbg::native_owned_instruction_semantics::decoder failed{target};
        auto const still_armed{fail_context_sized_new.exchange(false, ::std::memory_order_acq_rel)};
        require(!still_armed && failures.load(::std::memory_order_relaxed) == 2u && !failed,
            "owned target constructor catches allocation failure and supplies no provider");
    }
    dbg::native_owned_instruction_semantics::decoder recovered{target};
    require(bool(recovered), "constructor failure leaves RAII/LLVM registry usable for later cold decode");
    ::fast_io::io::println("native MC allocation: PASS default+owned-target catch and registry recovery; cold-DATA-only=yes kernel-native-qualification=no");
}

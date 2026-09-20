// Exercise the same preflight used by tailcall/byref, full/lazy, scalar/SIMD and fused interpreter stores.
#include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
#if defined(__unix__) || defined(__APPLE__)
extern "C"
{
#define main guarded_store_test_main
#include "../0014.llvm_jit/simd_guarded_store_fault.c"
#undef main
}
int main() { return guarded_store_test_main(); }
namespace memory_detail = ::uwvm2::runtime::compiler::uwvm_int::optable::details;
struct guarded_memory
{
    static constexpr bool can_mmap{true};
    ::std::byte* memory_begin;
    unsigned custom_page_size_log2{16u};
    constexpr bool require_dynamic_determination_memory_size() const noexcept { return false; }
};
template <unsigned Width>
void store_probe(unsigned char* base, uint32_t offset, unsigned char const* input)
{
    guarded_memory memory{reinterpret_cast<::std::byte*>(base)};
    auto pointer{memory_detail::prepare_memory_store_pointer<Width>(memory, offset)};
    ::std::memcpy(pointer, input, Width);
}
#define DEFINE(N) extern "C" void guarded_store_##N(unsigned char* base, uint32_t offset, const unsigned char* input) { store_probe<N>(base, offset, input); }
DEFINE(1) DEFINE(2) DEFINE(4) DEFINE(8) DEFINE(16) DEFINE(32) DEFINE(64)
#undef DEFINE
static_assert(memory_detail::wasm64_effective_offset(-1, 1).offset_65_bit);
static_assert(memory_detail::wasm64_effective_offset(-1, 1).offset == 0u);
static_assert(!memory_detail::wasm64_effective_offset(-1, 0).offset_65_bit);
static_assert(memory_detail::wasm64_effective_offset(-1, 0).offset == 0xffffffffffffffffull);
#else
int main() {}
#endif

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <fast_io.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/translate.h>

namespace
{
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    [[nodiscard]] bool one(::std::initializer_list<::std::uint8_t> bytes, bool expected) noexcept
    {
        // Temporary bytes own the full slice for this synchronous grammar-only call.
        // The empty module is not an initialized type/feature/validation authority.
        lazy::runtime_module_storage_t module{};
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes.begin())};
        auto const* const end{begin + bytes.size()};
        // [begin, end) is the live initializer-list allocation; end is one-past.
        // [safe      ] no token/module index is dereferenced by boundary discovery.
        auto cursor{begin};
        auto const result{lazy::details::skip_wasm_instruction_for_direct_call_scan(module,cursor,end,nullptr)};
        return result == expected && cursor == (expected ? end : begin);
    }
}
int main()
{
    ::std::size_t checks{};
    auto check{[&](bool value) noexcept { ++checks; return value; }};
    // Every current shorthand valtype is one byte, but 0x63/0x64 contain
    // a complete heap immediate. The scanner must not treat that heap as code.
    for(auto type : {0x7fu,0x7eu,0x7du,0x7cu,0x7bu,0x70u,0x6fu,
                     0x6eu,0x6du,0x6cu,0x6bu,0x6au,0x69u,0x71u,0x72u,0x73u,0x74u})
    { if(!check(one({0x1cu,0x01u,static_cast<::std::uint8_t>(type)},true))) { return 1; } }
    if(!check(one({0x1cu,0x01u,0x63u,0x00u},true)) ||
       !check(one({0x1cu,0x01u,0x64u,0x00u},true)) ||
       !check(one({0x1cu,0x01u,0x63u,0x80u,0x01u},true)) ||
       !check(one({0x1cu,0x01u,0x63u,0xffu,0xffu,0xffu,0xffu,0x0fu},true)) ||
       !check(one({0x1cu,0x01u,0x63u,0x69u},true)) ||
       !check(one({0x1cu,0x01u,0x64u,0x6bu},true)) ||
       !check(one({0x1cu,0x01u,0x63u,0x70u},true)) ||
       !check(one({0x1cu,0x01u,0x64u,0x6fu},true))) { return 2; }
    // Counts other than one are grammatical vectors. Only the real fused
    // validator decides select's arity and whether each indexed type exists.
    if(!check(one({0x1cu,0x00u},true)) ||
       !check(one({0x1cu,0x03u,0x63u,0x00u,0x69u,0x7fu},true))) { return 3; }
    // Neither a partial heap nor a malformed later element may commit outer
    // code_curr. Non-canonical signed spellings of abstract heaps are invalid.
    if(!check(one({0x1cu},false)) || !check(one({0x1cu,0x01u},false)) ||
       !check(one({0x1cu,0x01u,0x63u},false)) ||
       !check(one({0x1cu,0x01u,0x64u,0x80u},false)) ||
       !check(one({0x1cu,0x01u,0x63u,0xffu,0xffu,0xffu,0xffu,0x1fu},false)) ||
       !check(one({0x1cu,0x01u,0x63u,0x80u,0x80u,0x80u,0x80u,0x80u,0x00u},false)) ||
       !check(one({0x1cu,0x01u,0x63u,0xe9u,0x7fu},false)) ||
       !check(one({0x1cu,0x01u,0x65u},false)) ||
       !check(one({0x1cu,0x02u,0x7fu,0x64u,0x80u},false)) ||
       !check(one({0x1cu,0xffu,0xffu,0xffu,0xffu,0x0fu},false))) { return 4; }
    constexpr ::std::uint8_t stream[]{0x1cu,0x01u,0x63u,0x80u,0x01u,0x10u,0x01u,0x0bu};
    auto const* const begin{reinterpret_cast<::std::byte const*>(stream)};
    auto cursor{begin};
    auto const* const end{begin+sizeof(stream)};
    lazy::runtime_module_storage_t module{};
    for(auto offset : {5uz,7uz,8uz})
    {
        // Every expected endpoint lies in this fixed allocation or at its end.
        // [safe bytes ...] unsafe (one-past)
        //                 ^^ cursor after the scanner's complete immediate commit.
        if(!check(lazy::details::skip_wasm_instruction_for_direct_call_scan(module,cursor,end,nullptr) &&
                  cursor==begin+offset)) { return 5; }
    }
    ::fast_io::println("Core3 lazy valtype immediate checks=",checks);
}

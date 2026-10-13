#include <cstddef>
#include <cstdint>
#include <memory>
#include <initializer_list>
#include <fast_io.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/translate.h>

namespace
{
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    template <::std::size_t N>
    [[nodiscard]] bool one(::std::uint8_t const (&bytes)[N], bool expected,
                           ::std::size_t consumed) noexcept
    {
        // This is an immediate-grammar test, not an initialized-module authority.
        // None of these cases requires a concrete heap/type/table declaration.
        lazy::runtime_module_storage_t module{};
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        auto const* const end{begin + N};
        // [the N bytes owned by bytes] | end
        // [safe                     ] | one-past; begin/end share this allocation.
        auto cursor{begin};
        auto const accepted{lazy::details::skip_wasm_instruction_for_direct_call_scan(module, cursor, end, nullptr)};
        return accepted == expected && cursor == begin + consumed;
    }
}
int main()
{
    ::std::size_t checks{};
    auto check{[&](bool ok) noexcept { ++checks; return ok; }};
    constexpr ::std::uint8_t throw_zero[]{0x08u,0x00u};
    constexpr ::std::uint8_t throw_padded[]{0x08u,0x80u,0x00u};
    constexpr ::std::uint8_t throw_max[]{0x08u,0xffu,0xffu,0xffu,0xffu,0x0fu};
    constexpr ::std::uint8_t try_empty[]{0x1fu,0x40u,0x00u};
    constexpr ::std::uint8_t try_all_kinds[]{0x1fu,0x40u,0x04u,
        0x00u,0x80u,0x00u,0x00u, // catch padded-tag0 label0
        0x01u,0x00u,0x80u,0x00u, // catch_ref tag0 padded-label0
        0x02u,0x00u,             // catch_all label0
        0x03u,0x00u};            // catch_all_ref label0
    constexpr ::std::uint8_t try_nonnull_exn_result[]{0x1fu,0x64u,0x69u,0x00u};
    constexpr ::std::uint8_t throw_ref[]{0x0au};
    if(!check(one(throw_zero,true,sizeof(throw_zero))) ||
       !check(one(throw_padded,true,sizeof(throw_padded))) ||
       !check(one(throw_max,true,sizeof(throw_max))) ||
       !check(one(try_empty,true,sizeof(try_empty))) ||
       !check(one(try_all_kinds,true,sizeof(try_all_kinds))) ||
       !check(one(try_nonnull_exn_result,true,sizeof(try_nonnull_exn_result))) ||
       !check(one(throw_ref,true,sizeof(throw_ref)))) { return 1; }
    constexpr ::std::uint8_t missing_tag[]{0x08u};
    constexpr ::std::uint8_t partial_tag[]{0x08u,0x80u};
    constexpr ::std::uint8_t overflow_tag[]{0x08u,0xffu,0xffu,0xffu,0xffu,0x1fu};
    constexpr ::std::uint8_t missing_blocktype[]{0x1fu};
    constexpr ::std::uint8_t partial_heap[]{0x1fu,0x64u,0x80u};
    constexpr ::std::uint8_t missing_catch_count[]{0x1fu,0x40u};
    constexpr ::std::uint8_t missing_kind[]{0x1fu,0x40u,0x01u};
    constexpr ::std::uint8_t missing_tagged_label[]{0x1fu,0x40u,0x01u,0x00u,0x00u};
    constexpr ::std::uint8_t missing_all_label[]{0x1fu,0x40u,0x01u,0x02u};
    constexpr ::std::uint8_t bad_kind[]{0x1fu,0x40u,0x01u,0x04u,0x00u};
    constexpr ::std::uint8_t leb_kind_not_literal[]{0x1fu,0x40u,0x01u,0x80u,0x00u,0x00u};
    constexpr ::std::uint8_t oversized_count[]{0x1fu,0x40u,0xffu,0xffu,0xffu,0xffu,0x0fu};
    if(!check(one(missing_tag,false,0uz)) || !check(one(partial_tag,false,0uz)) ||
       !check(one(overflow_tag,false,0uz)) || !check(one(missing_blocktype,false,0uz)) ||
       !check(one(partial_heap,false,0uz)) || !check(one(missing_catch_count,false,0uz)) ||
       !check(one(missing_kind,false,0uz)) || !check(one(missing_tagged_label,false,0uz)) ||
       !check(one(missing_all_label,false,0uz)) || !check(one(bad_kind,false,0uz)) ||
       !check(one(leb_kind_not_literal,false,0uz)) || !check(one(oversized_count,false,0uz))) { return 2; }
    // Immediates must not eat the next opcode. This stream has a direct call
    // after try_table, another after throw and the implicit function end.
    constexpr ::std::uint8_t stream[]{0x1fu,0x40u,0x01u,0x00u,0x00u,0x00u,
        0x10u,0x01u,0x0bu,0x08u,0x00u,0x10u,0x02u,0x0bu};
    lazy::runtime_module_storage_t module{};
    auto const* const begin{reinterpret_cast<::std::byte const*>(stream)};
    auto cursor{begin};
    auto const* const end{begin+sizeof(stream)};
    for(auto expected : {6uz,8uz,9uz,11uz,13uz,14uz})
    {
        if(!check(lazy::details::skip_wasm_instruction_for_direct_call_scan(module,cursor,end,nullptr) &&
                  cursor == begin+expected)) { return 3; }
    }
    if(!check(cursor==end)) { return 4; }
    ::fast_io::println("Core3 lazy EH immediate checks=",checks);
}

#include <uwvm2/parser/wasm/standard/wasm3/type/value_type.h>
#include <uwvm2/object/global/ref.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <type_traits>

using i31 = ::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31;
using global_ref = ::uwvm2::object::global::wasm_global_ref_t;
using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
static_assert(sizeof(i31) == 4);
static_assert(::std::is_standard_layout_v<i31> && ::std::is_trivially_copyable_v<i31>);
static_assert(sizeof(global_ref) == 2 * sizeof(void*));
static_assert(offsetof(global_ref, kind) == sizeof(void*));
static_assert(::std::is_standard_layout_v<global_ref> && ::std::is_trivially_copyable_v<global_ref>);
static_assert(::uwvm2::object::global::make_wasm_i31_reference(-1).kind == ref_kind::wasm_i31);
static_assert(i31::from_i32(0x4000'0000).get_s() == -0x4000'0000);
static_assert(i31::from_i32(0x4000'0000).get_u() == 0x4000'0000u);
static_assert(i31::from_i32(::std::bit_cast<::std::int32_t>(0x8000'0000u)).get_u() == 0);

extern "C" [[gnu::used, gnu::noinline]] ::std::int32_t gc_i31_signed(::std::int32_t value) noexcept
{ return i31::from_i32(value).get_s(); }
extern "C" [[gnu::used, gnu::noinline]] ::std::uint32_t gc_i31_unsigned(::std::int32_t value) noexcept
{ return i31::from_i32(value).get_u(); }

extern "C" [[gnu::used, gnu::noinline]] ::std::int32_t gc_i31_ref_signed(::std::int32_t value) noexcept
{ return ::uwvm2::object::global::make_wasm_i31_reference(value).storage.wasm_i31.get_s(); }

int main()
{
    struct row { ::std::int32_t input, signed_result; ::std::uint32_t unsigned_result; };
    constexpr ::std::array cases{
        row{0, 0, 0u},
        row{1, 1, 1u},
        row{0x3fff'ffff, 0x3fff'ffff, 0x3fff'ffffu},
        row{0x4000'0000, -0x4000'0000, 0x4000'0000u},
        row{0x7fff'ffff, -1, 0x7fff'ffffu},
        row{-1, -1, 0x7fff'ffffu},
        row{::std::bit_cast<::std::int32_t>(0x8000'0000u), 0, 0u},
        row{::std::bit_cast<::std::int32_t>(0x8000'0001u), 1, 1u},
    };
    for(auto const test : cases)
    {
        auto const ref{::uwvm2::object::global::make_wasm_i31_reference(test.input)};
        if(gc_i31_signed(test.input) != test.signed_result || gc_i31_unsigned(test.input) != test.unsigned_result ||
           gc_i31_ref_signed(test.input) != test.signed_result || ref.kind != ref_kind::wasm_i31 ||
           ref.storage.wasm_i31.get_s() != test.signed_result || ref.storage.wasm_i31.get_u() != test.unsigned_result)
        { ::std::fputs("FAIL Core 3 i31 representation\n", stderr); return 1; }
    }
    ::std::puts("PASS Core 3 i31 representation: 8 boundary values");
}

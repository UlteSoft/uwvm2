// Import-only consumer: no source_dwarf header can conceal a missing export.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
import fast_io;
import uwvm2.uwvm.debugger;
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{
    if(!value)
    {
        ::fast_io::io::perrln("debug_source_dwarf_module: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
template<typename Char> static void escaped_output()
{
    ::std::basic_string<Char> output{}, expected{};
    // [fixed owned string objects] end
    // [safe                      ] each formatter borrows its own live object.
    //  ^^ no borrowed metadata/native/guest pointer is retained in either result.
    ::fast_io::basic_ostring_ref_std<Char> stream{::std::addressof(output)};
    ::fast_io::basic_ostring_ref_std<Char> expected_stream{::std::addressof(expected)};
    ::fast_io::io::print(stream, escaped_metadata_text{::std::string_view{"A\x1b\n"}});
    ::fast_io::io::print(expected_stream, ::fast_io::mnp::code_cvt(::std::string_view{"A\\x1b\\x0a"}));
    check(output == expected, "imported formatter and fast_io stdstring bridge escape every character domain");
}
int main()
{
    escaped_output<char>(); escaped_output<wchar_t>(); escaped_output<char8_t>();
    escaped_output<char16_t>(); escaped_output<char32_t>();
    ::std::vector<type_record> types(1u);
    auto& scalar{types[0u]}; scalar.name = ::fast_io::concat_std(::std::string_view{"int"});
    scalar.kind = type_kind::scalar; scalar.encoding = 5u; scalar.byte_count = 4u;
    scalar.byte_size = 4u; scalar.size_known = true;
    ::std::array<::std::byte, 4u> const copied{::std::byte{0xf9u}, ::std::byte{0xffu}, ::std::byte{0xffu}, ::std::byte{0xffu}};
    ::std::vector<object_node> objects{};
    check(query_object_value(types, 0u, copied, objects) == inline_query_error::none && objects.size() == 1u &&
          objects[0u].value_available && objects[0u].bits == 0xfffffff9u, "imported scalar scanner preserves DWARF little endian");
    object_selector selector{};
    check(parse_source_object_selector("outer::value[-9223372036854775808]", selector) == object_selector_error::none &&
          selector.root_name == "outer::value" && selector.steps.size() == 1u, "imported decimal parser and owned selector strings");
    ::std::array<::std::byte, 6u> const expression{::std::byte{0xedu}, ::std::byte{}, ::std::byte{},
        ::std::byte{0x9fu}, ::std::byte{0x93u}, ::std::byte{4u}};
    auto const plan{decode_location_plan(expression, 4u, {})};
    ::std::array<copied_numeric_local, 1u> const locals{};
    composite_value pieces{};
    check(plan.kind == plan_kind::composite_value && materialize_location_pieces(plan, locals, 1u, {}, 4u, pieces) == piece_query_error::none &&
          !pieces.fully_available && pieces.piece_reasons.size() == 1u && pieces.piece_reasons[0u] == piece_unavailable_reason::local_unavailable,
          "imported default-false local availability cannot manufacture piece bits");
    ::std::vector<inline_frame> frames{};
    check(query_inline_frames({}, 0u, frames) == inline_query_error::unavailable && frames.empty(), "imported shared concrete scope query remains fail closed");
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
    static_assert(!::std::is_copy_constructible_v<::uwvm2::uwvm::debugger::source_dwarf::index> &&
                  !::std::is_move_constructible_v<::uwvm2::uwvm::debugger::source_dwarf::index>);
#endif
    ::fast_io::io::println("PASS focused imported DWARF partitions, scalar/decimal scan, stdstring formatting and explicit unavailable pieces");
}

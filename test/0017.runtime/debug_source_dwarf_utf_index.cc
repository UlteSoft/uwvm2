// Consumes actual Clang/Rust producer Wasm DWARF through the production index.
// Scalar carrier/guest-byte inputs below are OWNED copies, not read authority.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = uwvm2::uwvm::debugger::source_dwarf::index;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_utf_index: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main(int argc, char const* const* argv)
{
    check(argc == 3, "require actual linked Wasm UTF fixture and cpp/rust label");
    ::std::string_view language{argv[2]};
    check(language == "cpp" || language == "rust", "supported producer label");
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    check(file.size() >= 8u, "Wasm header bounds");
    // [RAII loader bytes ... header (8) ... module_end]
    // [safe                                        ] unsafe (one-past)
    //  ^^ size checked before bounded span or header reads.
    auto const bytes{::std::span<::std::byte const>{reinterpret_cast<::std::byte const*>(file.data()), file.size()}};
    ::std::array<unsigned char, 8u> const magic{0u, 0x61u, 0x73u, 0x6du, 1u, 0u, 0u, 0u};
    for(::std::size_t i{}; i != magic.size(); ++i) { check(::std::to_integer<unsigned char>(bytes[i]) == magic[i], "valid Wasm version1 header"); }
    details::reader module{bytes, 8u}; ::std::vector<section> sections{}; ::std::uint64_t code_size{};
    while(module.cursor != module.bytes.size())
    {
        ::std::uint8_t id{}; ::std::uint32_t size{};
        check(module.byte(id) && module.leb(size) && size <= module.bytes.size() - module.cursor, "bounded section payload");
        // [safe] checked size before subspan and scalar cursor advance.
        auto const payload{module.bytes.subspan(module.cursor, size)}; module.cursor += size;
        if(id == 10u) { check(code_size == 0u, "one code section"); code_size = size; }
        if(id != 0u) { continue; }
        details::reader custom{payload}; ::std::uint32_t length{};
        check(custom.leb(length) && length <= payload.size() - custom.cursor, "bounded custom-section name");
        // [custom payload ... cursor ... cursor+length ... end]
        // [safe                                              ] unsafe (one-past)
        //                    ^^ checked name length precedes pointer derivation.
        ::std::string_view const name{reinterpret_cast<char const*>(payload.data() + custom.cursor), length};
        custom.cursor += length; // checked scalar advance may reach payload_end.
        if(name.starts_with(".debug_") || name == "external_debug_info") { sections.push_back({name, payload.subspan(custom.cursor)}); }
    }
    check(code_size != 0u, "real code section present"); ::std::unique_ptr<metadata_index> parsed{};
    auto const status{metadata_index::parse({sections, code_size, 4u}, parsed)};
    if(status != error::none) { ::fast_io::io::perrln("parse status=", ::fast_io::mnp::dec(static_cast<unsigned>(status))); }
    check(status == error::none && parsed, "real producer embedded DWARF parsed");
    auto const types{parsed->types()}; variable_record const* object{};
    for(auto const& candidate : parsed->variables())
    {
        if(candidate.name != "object" || candidate.type >= types.size()) { continue; }
        auto const& type{types[candidate.type]};
        if(type.kind != type_kind::structure && type.kind != type_kind::class_type) { continue; }
        check(object == nullptr, "exactly one actual local UTF aggregate"); object = ::std::addressof(candidate);
    }
    check(object != nullptr && !object->locations.empty(), "real producer retains local aggregate and location metadata");
    auto const& aggregate{types[object->type]};
    check(aggregate.size_known && aggregate.byte_size == 12u, "actual Wasm32 C++/Rust UTF aggregate layout");
    auto const scalar_member{[&](::std::string_view name, ::std::uint8_t width, ::std::uint64_t offset)
    {
        type_record const* selected{};
        for(auto const& field : aggregate.members)
        {
            if(field.name != name) { continue; }
            check(selected == nullptr && field.offset_known && field.byte_offset == offset && field.type < types.size(),
                  "actual UTF field identity, offset and type reference");
            selected = ::std::addressof(types[field.type]);
        }
        check(selected != nullptr && selected->kind == type_kind::scalar && selected->encoding == 0x10u && selected->byte_count == width,
              "actual compiler emits DW_ATE_UTF with exact source unit width");
        check(value_details::classify(*selected) == numeric_kind::unsigned_integer, "actual UTF metadata reaches production scalar classifier");
    }};
    if(language == "cpp") { scalar_member("octet", 1u, 0u); scalar_member("unit", 2u, 2u); scalar_member("point", 4u, 4u); }
    else { scalar_member("point", 4u, 0u); }
    type_record const* pair{};
    for(auto const& field : aggregate.members)
    {
        if(field.name != "pair") { continue; }
        check(pair == nullptr && field.offset_known && field.byte_offset == (language == "cpp" ? 8u : 4u) && field.type < types.size(),
              "actual UTF array field offset and type identity"); pair = ::std::addressof(types[field.type]);
    }
    check(pair != nullptr && pair->kind == type_kind::array && pair->referenced_type < types.size() && pair->dimensions.size() == 1u &&
          pair->dimensions.front().count_known && pair->dimensions.front().count == 2u, "actual UTF array is finite two-element metadata");
    auto const& element{types[pair->referenced_type]};
    check(element.kind == type_kind::scalar && element.encoding == 0x10u && element.byte_count == (language == "cpp" ? 2u : 4u),
          "actual UTF array element encoding and width");
    ::std::array<::std::byte, 12u> const cpp_bytes{::std::byte{0x80u}, ::std::byte{}, ::std::byte{0xbbu}, ::std::byte{3u},
        ::std::byte{0x42u}, ::std::byte{0xf6u}, ::std::byte{1u}, ::std::byte{}, ::std::byte{0x3du}, ::std::byte{0xd8u}, ::std::byte{0x42u}, ::std::byte{0xdeu}};
    ::std::array<::std::byte, 12u> const rust_bytes{::std::byte{0x42u}, ::std::byte{0xf6u}, ::std::byte{1u}, ::std::byte{},
        ::std::byte{0xbbu}, ::std::byte{3u}, ::std::byte{}, ::std::byte{}, ::std::byte{0xffu}, ::std::byte{0xffu}, ::std::byte{0x10u}, ::std::byte{}};
    auto const copied{::std::span<::std::byte const>{language == "cpp" ? cpp_bytes : rust_bytes}};
    ::std::vector<object_node> out{};
    check(query_object_value(types, object->type, copied, out) == inline_query_error::none && out.size() == (language == "cpp" ? 7u : 5u),
          "actual compiler type graph expands exact owned UTF object bytes");
    ::std::size_t units{};
    for(auto const& node : out)
    {
        if(node.kind != type_kind::scalar) { continue; }
        check(node.value_available && node.scalar_kind == numeric_kind::unsigned_integer, "every actual UTF leaf is readable as unsigned code-unit bits");
        ++units;
        if(node.name == "point") { check(node.bits == 0x1f642u, "actual C++/Rust UTF32 type preserves supplementary Unicode scalar"); }
        if(node.name == "octet") { check(node.bits == 0x80u, "actual C++ UTF8 code unit is unsigned"); }
        if(node.name == "unit") { check(node.bits == 0x3bbu, "actual C++ UTF16 code unit uses Wasm little endian"); }
        if(node.name == "[0]") { check(node.bits == (language == "cpp" ? 0xd83du : 0x3bbu), "actual UTF array first element uses exact source width"); }
        if(node.name == "[1]") { check(node.bits == (language == "cpp" ? 0xde42u : 0x10ffffu), "actual UTF array last element remains bounded and unsigned"); }
    }
    check(units == (language == "cpp" ? 5u : 3u), "actual type graph contains exactly the expected UTF leaves");
    ::fast_io::io::println("PASS actual ", ::fast_io::mnp::code_cvt(language), " DW_ATE_UTF widths/layout and owned copied object values");
}

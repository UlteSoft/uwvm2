// This executable consumes REAL Clang/Rust DWARF from the three fixture sources.
// A synthetic layout test cannot qualify these producer/ABI expectations.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <uwvm2/uwvm/debugger/source_dwarf_selectors.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = uwvm2::uwvm::debugger::source_dwarf::index;
static void check(bool condition, char const* message)
{ if(!condition) { ::fast_io::io::perrln("debug_source_dwarf_objects_index: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main(int argc, char const* const* argv)
{
    check(argc == 3, "require real Wasm fixture path and c/cpp/rust producer label");
    ::std::string_view language{argv[2]};
    check(language == "c" || language == "cpp" || language == "rust" || language == "cpp-pieces", "supported producer label");
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
    if(language == "cpp-pieces")
    {
        bool actual_composite{};
        for(auto const& variable : parsed->variables())
        {
            if(variable.name != "fragments") { continue; }
            for(auto const& location : variable.locations)
            {
                ::fast_io::io::println("actual-fragments location-kind=", ::fast_io::mnp::dec(static_cast<unsigned>(location.plan.kind)),
                    " reason=", ::fast_io::mnp::dec(static_cast<unsigned>(location.plan.reason)),
                    " pieces=", ::fast_io::mnp::dec(location.plan.pieces.size()));
                if(location.plan.kind != plan_kind::composite_value || location.plan.pieces.size() < 2u) { continue; }
                check(location.plan.composite_bits == 64u, "real two-int aggregate covers exactly 64 object bits");
                for(auto const& piece : location.plan.pieces)
                { check(piece.atom.reason == unavailable_reason::none &&
                        (piece.atom.kind == plan_kind::wasm_local_value || piece.atom.kind == plan_kind::constant_value),
                        "real optimized producer pieces are supported finite copied-value atoms"); }
                actual_composite = true;
            }
        }
        check(actual_composite, "actual optimized producer must emit supported composite locations; source intent is insufficient");
        ::fast_io::io::println("PASS actual optimized C++ producer DW_OP_piece/bit_piece metadata"); return 0;
    }
    variable_record const* object{};
    for(auto const& variable : parsed->variables())
    {
        if(variable.name != "object" || variable.type >= parsed->types().size()) { continue; }
        auto const kind{parsed->types()[variable.type].kind};
        if(kind == type_kind::structure || kind == type_kind::class_type)
        { check(object == nullptr, "one concrete object variable"); object = ::std::addressof(variable); }
    }
    check(object != nullptr, "real stack aggregate variable retained");
    auto const types{parsed->types()}; auto const& type{types[object->type]};
    check(type.size_known && type.byte_size == (language == "cpp" ? 40u : 16u), "actual Wasm32 producer object ABI size");
    bool has_array{}, has_enum{}, has_pointer{}, has_base{}, has_signed_bit{}, has_unsigned_bit{};
    for(auto const& member : type.members)
    {
        check(member.type < types.size(), "member refers to an interned type"); auto const& child{types[member.type]};
        if(member.name == "lanes")
        { check(member.byte_offset == 4u && child.kind == type_kind::array && child.dimensions.size() == 1u &&
                child.dimensions[0u].count_known && child.dimensions[0u].count == 3u, "actual C/Rust fixed array layout"); has_array = true; }
        if(member.name == "grid")
        { check(member.byte_offset == 4u && child.kind == type_kind::array && child.dimensions.size() == 2u &&
                child.dimensions[0u].count == 2u && child.dimensions[1u].count == 3u, "actual C++ multidimensional array layout"); has_array = true; }
        if(member.name == "color" || member.name == "shade")
        { check(child.kind == type_kind::enumeration && child.byte_count == 4u && !child.enumerators.empty(), "real enum symbols and underlying width"); has_enum = true; }
        if(member.name == "next")
        { check(child.kind == type_kind::pointer && child.referenced_type < types.size() &&
                types[child.referenced_type].kind == type.kind && types[child.referenced_type].byte_size == type.byte_size &&
                member.byte_offset == (language == "cpp" ? 36u : 12u), "actual self-referential pointer remains metadata edge through typedef/const"); has_pointer = true; }
        if(member.inherited) { check(member.offset_known && member.byte_offset == 0u, "constant nonvirtual C++ base offset"); has_base = true; }
        if(member.name == "signed_bits") { check(member.bit_field && member.offset_known && member.bit_size == 5u, "real signed DWARF5 bitfield"); has_signed_bit = true; }
        if(member.name == "flag_bits") { check(member.bit_field && member.offset_known && member.bit_size == 7u, "real unsigned DWARF5 bitfield"); has_unsigned_bit = true; }
    }
    check(has_array && has_enum && has_pointer, "real producer aggregate covers array, enum and recursive pointer");
    if(language == "cpp") { check(has_base && has_signed_bit && has_unsigned_bit, "real C++ base/bitfield information"); }
    bool fbreg{}; for(auto const& location : object->locations) { fbreg |= location.plan.kind == plan_kind::frame_relative_offset; }
    check(fbreg, "real O0 producer stack aggregate DW_OP_fbreg");
    ::std::vector<object_node> layout{};
    check(query_type_layout(types, object->type, layout) == inline_query_error::none && layout.size() >= 8u, "real type graph expands without pointer dereference");
    object_selector selector{};
    auto const expression{language == "cpp" ? ::std::string_view{"object.grid[1][2]"} : ::std::string_view{"object.lanes[2]"}};
    check(parse_source_object_selector(expression, selector) == object_selector_error::none, "real producer bounded field/array selector");
    ::std::vector<object_node> selected_layout{};
    check(query_selected_object_type(types, object->type, selector.steps, selected_layout) == inline_query_error::none && selected_layout.size() == 1u &&
          selected_layout.front().kind == type_kind::scalar && selected_layout.front().byte_offset == (language == "cpp" ? 24u : 6u) &&
          selected_layout.front().byte_size == (language == "cpp" ? 4u : 1u), "real C/C++/Rust member and array metadata selector exact offset/width");
    if(language == "cpp")
    {
        check(parse_source_object_selector("object.base", selector) == object_selector_error::none &&
              query_selected_object_type(types, object->type, selector.steps, selected_layout) == inline_query_error::none && selected_layout.front().byte_offset == 0u,
              "real C++ inherited member resolves through constant base");
        check(parse_source_object_selector("object.signed_bits", selector) == object_selector_error::none &&
              query_selected_object_type(types, object->type, selector.steps, selected_layout) == inline_query_error::none && selected_layout.front().bit_field && selected_layout.front().bit_width == 5u,
              "real C++ bitfield selector retains original owner metadata");
    }
    check(parse_source_object_selector("object.next.value", selector) == object_selector_error::none &&
          query_selected_object_type(types, object->type, selector.steps, selected_layout) == inline_query_error::unavailable && selected_layout.empty(),
          "real producer pointer remains metadata and never an implicit dereference");
    for(auto const& node : layout) { ::fast_io::io::println(object_details_of(node)); }
    ::fast_io::io::println("PASS actual ", ::fast_io::mnp::code_cvt(language), " stack aggregate DWARF layout, enum, array and pointer metadata");
}

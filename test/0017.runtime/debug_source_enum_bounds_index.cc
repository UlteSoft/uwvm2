// This executable consumes original Clang C/Objective-C/C++/Objective-C++ DWARF.
// A synthetic layout test cannot qualify these producer/ABI expectations.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <uwvm2/uwvm/debugger/source_dwarf_selectors.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = uwvm2::uwvm::debugger::source_dwarf::index;
static void check(bool condition, char const* message)
{ if(!condition) { ::fast_io::io::perrln("debug_source_enum_bounds_index: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main(int argc, char const* const* argv)
{
    check(argc == 3, "require original Wasm path and address width");
    unsigned width{};
    ::std::string_view width_text{argv[2]};
    auto const parsed_width{::fast_io::parse_by_scan(width_text.data(), width_text.data() + width_text.size(), width)};
    check(parsed_width.code == ::fast_io::parse_code::ok && parsed_width.iter == width_text.data() + width_text.size() && (width == 4u || width == 8u), "actual producer address width");
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
    auto const status{metadata_index::parse({sections, code_size, static_cast<::std::uint8_t>(width)}, parsed)};
    if(status != error::none) { ::fast_io::io::perrln("parse status=", ::fast_io::mnp::dec(static_cast<unsigned>(status))); }
    check(status == error::none && parsed, "real producer embedded DWARF parsed");
    variable_record const* object{};
    for(auto const& variable : parsed->variables())
    {
        if(variable.name == "object" && variable.type < parsed->types().size())
        { check(object == nullptr, "one original aggregate local"); object = ::std::addressof(variable); }
    }
    check(object != nullptr, "real producer aggregate retained");
    auto const types{parsed->types()};
    auto const& aggregate{types[object->type]};
    type_record const* grid{};
    for(auto const& member : aggregate.members)
    {
        if(member.name == "grid" && member.type < types.size())
        { check(grid == nullptr, "one real grid"); grid = ::std::addressof(types[member.type]); }
    }
    check(grid != nullptr && grid->kind == type_kind::array && grid->dimensions.size() == 2u, "real two-dimensional array");
    for(::std::size_t i{}; i != 2u; ++i)
    {
        auto const& dimension{grid->dimensions[i]};
        check(dimension.lower_bound_known && dimension.lower_bound == 0 &&
              dimension.count_known && dimension.count == i + 2u, "actual producer missing lower-bound defaults to zero");
    }
    object_selector selector{};
    check(parse_source_object_selector("object.grid[1][2]", selector) == object_selector_error::none, "finite existing array selector");
    ::std::vector<object_node> layout{};
    check(query_selected_object_type(types, object->type, selector.steps, layout) == inline_query_error::none &&
          layout.size() == 1u && layout[0u].kind == type_kind::scalar && layout[0u].byte_size == 4u,
          "actual selected last element metadata after default-bound fix");
    ::fast_io::io::println("PASS actual C/Objective-C/C++/Objective-C++ enum fixture array bounds and selector width=", width);
}

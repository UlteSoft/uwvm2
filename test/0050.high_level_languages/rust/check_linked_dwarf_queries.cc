// Consume an actual linked Rust/std Wasm; this does not execute guest code.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>

namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
static void require(bool value, char const* message)
{
    if(!value)
    {
        ::fast_io::io::perrln("linked Rust DWARF: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}

int main(int argc, char const* const* argv)
{
    require(argc == 3, "require Wasm path and its recorded compilation directory");
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    require(file.size() >= 8u, "bounded Wasm header");
    auto bytes{::std::span<::std::byte const>{reinterpret_cast<::std::byte const*>(file.data()), file.size()}};
    ::std::array<unsigned char, 8u> const magic{0u, 0x61u, 0x73u, 0x6du, 1u, 0u, 0u, 0u};
    for(::std::size_t i{}; i != magic.size(); ++i)
    { require(::std::to_integer<unsigned char>(bytes[i]) == magic[i], "Wasm version 1"); }
    dwarf::details::reader module{bytes, 8u};
    ::std::vector<dwarf::section> sections{};
    ::std::uint64_t code_size{};
    while(module.cursor != module.bytes.size())
    {
        ::std::uint8_t id{}; ::std::uint32_t size{};
        require(module.byte(id) && module.leb(size) && size <= module.bytes.size() - module.cursor, "section bounds");
        auto const payload{module.bytes.subspan(module.cursor, size)};
        module.cursor += size;
        if(id == 10u) { require(code_size == 0u, "one code section"); code_size = size; }
        if(id != 0u) { continue; }
        dwarf::details::reader custom{payload}; ::std::uint32_t length{};
        require(custom.leb(length) && length <= payload.size() - custom.cursor, "custom name bounds");
        ::std::string_view name{reinterpret_cast<char const*>(payload.data() + custom.cursor), length};
        custom.cursor += length;
        if(name.starts_with(".debug_") || name == "external_debug_info")
        { sections.push_back({name, payload.subspan(custom.cursor)}); }
    }
    require(code_size != 0u, "code section present");
    dwarf::image_input const input{sections, code_size, 4u};
    ::std::unique_ptr<dwarf::index> parsed{};
    auto accept = [&]
    {
        require(dwarf::index::parse(input, parsed) == dwarf::error::none && parsed, "default budget accepts real linked metadata");
        bool directory{}, symbol{};
        for(auto const& row : parsed->line_directories()) { directory |= row.directory == argv[2]; }
        for(auto const& row : parsed->scopes()) { symbol |= row.name == "debug_value"; }
        require(directory && symbol && !parsed->variables().empty(), "real CU directory, symbol and variable records");
    };
    accept();
    dwarf::numeric_query_limits numeric{};
    ::std::size_t strings{}, locations{};
    require(dwarf::value_details::validate_numeric_metadata(parsed->scopes(), parsed->types(), parsed->variables(),
        numeric, strings, locations) == dwarf::inline_query_error::none, "default numeric metadata budget accepts linked std");
    require(strings > 1024u * 1024u && locations != 0u, "fixture exceeds legacy cumulative string budget");
    ::fast_io::io::println("PASS default scalar metadata schema");
    auto legacy{numeric}; legacy.max_metadata_string_bytes = 1024u * 1024u;
    require(dwarf::value_details::validate_numeric_metadata(parsed->scopes(), parsed->types(), parsed->variables(),
        legacy, strings, locations) == dwarf::inline_query_error::limit_exceeded, "legacy scalar metadata budget refuses");
    ::fast_io::io::println("PASS legacy scalar metadata budget refuses");
    auto const* numeric_owner{parsed->numeric_validation()};
    require(numeric_owner && numeric_owner->accepts(parsed->scopes(), parsed->types(), parsed->variables(), numeric),
        "own immutable scalar schema proof accepts");
    require(!numeric_owner->accepts(parsed->scopes(), parsed->types(), parsed->variables(), legacy),
        "cached proof cannot bypass a smaller string budget");
    auto no_locations{numeric}; no_locations.max_locations = 0u;
    require(!numeric_owner->accepts(parsed->scopes(), parsed->types(), parsed->variables(), no_locations),
        "cached proof cannot bypass a smaller location budget");
    ::std::vector<dwarf::type_record> foreign{parsed->types().begin(), parsed->types().end()};
    require(!numeric_owner->accepts(parsed->scopes(), foreign, parsed->variables(), numeric),
        "foreign copied types cannot borrow scalar proof");
    ::fast_io::io::println("PASS scalar schema proof enforces ownership and limits");
    dwarf::object_query_limits object{};
    require(dwarf::object_details::validate_uncached(parsed->types(), object) == dwarf::inline_query_error::none,
        "object schema fits unchanged defaults");
    auto const* object_owner{parsed->object_validation()};
    dwarf::inline_query_error status{dwarf::inline_query_error::malformed};
#if defined(UWVM_USE_THREAD_LOCAL)
    require(object_owner && object_owner->lookup(parsed->types(), object, status) && status == dwarf::inline_query_error::none,
        "own immutable object schema proof accepts");
#else
    require(!object_owner, "thread-local disabled path has no borrowed object proof");
#endif
    ::fast_io::io::println("PASS object schema fits unchanged defaults");
    auto no_types{object}; no_types.max_types = 0u;
#if defined(UWVM_USE_THREAD_LOCAL)
    require(!object_owner->lookup(parsed->types(), no_types, status) &&
        dwarf::object_details::validate_uncached(parsed->types(), no_types) == dwarf::inline_query_error::limit_exceeded,
        "cached object proof cannot bypass narrowed schema limits");
    require(!object_owner->lookup(foreign, object, status), "foreign copied types cannot borrow object proof");
#else
    require(dwarf::object_details::validate_uncached(parsed->types(), no_types) == dwarf::inline_query_error::limit_exceeded,
        "uncached object path enforces narrowed schema limits");
    require(dwarf::object_details::validate_uncached(foreign, object) == dwarf::inline_query_error::none,
        "foreign copied types undergo ordinary schema validation");
#endif
    ::fast_io::io::println("PASS object schema proof enforces ownership and limits");
#if defined(UWVM_USE_THREAD_LOCAL)
    ::fast_io::io::println("THREAD_LOCAL_OBJECT_PROOF enabled");
#else
    ::fast_io::io::println("THREAD_LOCAL_OBJECT_PROOF disabled");
#endif
    ::fast_io::io::println("NO_GUEST_EXECUTION_OR_VALUE_QUERY");
}

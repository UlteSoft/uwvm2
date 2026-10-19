// Consume an actual linked Rust/std Wasm; this does not execute guest code.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
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
    ::fast_io::io::println("PASS default linked metadata");
    auto reject = [&](dwarf::image_input image, dwarf::limits cap, dwarf::error expected, char const* label)
    {
        if(!parsed) { accept(); }
        require(dwarf::index::parse(image, parsed, cap) == expected && !parsed, "rejection clears previously published metadata");
        ::fast_io::io::println("PASS ", ::fast_io::mnp::os_c_str(label));
    };
    dwarf::limits cap{}; cap.max_dies = 65536u;
    reject(input, cap, dwarf::error::limit_exceeded, "legacy DIE bound rejects");
    cap = {}; cap.max_total_string_bytes = 1024u * 1024u;
    reject(input, cap, dwarf::error::limit_exceeded, "legacy string bound rejects");
    cap = {}; cap.max_section_bytes = 512u * 1024u;
    reject(input, cap, dwarf::error::limit_exceeded, "explicit section bound rejects");
    auto truncated{sections}; bool changed{};
    for(auto& section : truncated)
    {
        if(section.name == ".debug_info")
        {
            require(!section.payload.empty(), "nonempty debug info");
            section.payload = section.payload.first(section.payload.size() - 1u); changed = true;
        }
    }
    require(changed, "debug info section present");
    reject({truncated, code_size, 4u}, {}, dwarf::error::malformed, "truncated unit rejects");
    accept();
    ::fast_io::io::println("PASS clean reparse after refusals");
}

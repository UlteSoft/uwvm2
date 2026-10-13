#include <uwvm2/uwvm/debugger/source_map.h>
#include <fast_io.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace uwvm2::uwvm::debugger;
using bytes = std::vector<std::byte>;

[[noreturn]] static void fail(char const* message)
{
    ::fast_io::io::perrln("debug_source_map: ", ::fast_io::mnp::os_c_str(message));
    std::exit(1);
}
static void check(bool condition, char const* message)
{ if(!condition) { fail(message); } }
static void check_leb_edges()
{
    using uwvm2::uwvm::debugger::source_map_details::reader;
    bytes maximum_unsigned(10u, std::byte{0xff}); maximum_unsigned.back() = std::byte{0x01};
    reader unsigned_reader{maximum_unsigned};
    std::uint64_t unsigned_value{};
    check(unsigned_reader.uleb(unsigned_value) &&
          unsigned_value == (std::numeric_limits<std::uint64_t>::max)() &&
          unsigned_reader.cursor == 10u, "u64 maximum LEB");
    maximum_unsigned.back() = std::byte{0x02};
    reader unsigned_overflow{maximum_unsigned};
    check(!unsigned_overflow.uleb(unsigned_value), "u64 overflow LEB rejected");
    bytes minimum_signed(10u, std::byte{0x80}); minimum_signed.back() = std::byte{0x7f};
    reader signed_reader{minimum_signed};
    std::int64_t signed_value{};
    check(signed_reader.sleb(signed_value) &&
          signed_value == (std::numeric_limits<std::int64_t>::min)() &&
          signed_reader.cursor == 10u, "i64 minimum LEB");
    minimum_signed.back() = std::byte{0x01};
    reader signed_overflow{minimum_signed};
    check(!signed_overflow.sleb(signed_value), "i64 overflow LEB rejected");
    bytes truncated{std::byte{0x80}};
    reader truncated_reader{truncated};
    check(!truncated_reader.uleb(unsigned_value), "truncated LEB rejected");
}
static void put(bytes& out, std::uint64_t value, unsigned width)
{ for(unsigned i{}; i != width; ++i) { out.push_back(std::byte(value >> (8u * i))); } }
static void uleb(bytes& out, std::uint64_t value)
{
    do { auto const part{std::uint8_t(value & 0x7fu)}; value >>= 7u;
         put(out, part | (value ? 0x80u : 0u), 1u); } while(value);
}
static void sleb(bytes& out, std::int64_t value)
{
    for(;;)
    {
        auto const part{std::uint8_t(value & 0x7f)};
        value >>= 7;
        bool const done{(value == 0 && !(part & 0x40u)) || (value == -1 && (part & 0x40u))};
        put(out, part | (done ? 0u : 0x80u), 1u);
        if(done) { return; }
    }
}
static void cstr(bytes& out, std::string_view value)
{
    for(char c : value) { put(out, static_cast<unsigned char>(c), 1u); }
    put(out, 0u, 1u);
}
static void append(bytes& out, bytes const& other)
{ out.insert(out.end(), other.begin(), other.end()); }
static bytes base_header()
{
    bytes h;
    put(h, 1u, 1u); // min instruction length
    put(h, 1u, 1u); // max operations per instruction
    put(h, 1u, 1u); // default is_stmt
    put(h, 0xfbu, 1u); // line_base=-5
    put(h, 14u, 1u); // line_range
    put(h, 13u, 1u); // opcode_base
    for(unsigned i{1u}; i != 13u; ++i)
    { put(h, i == 2u || i == 3u || i == 4u || i == 5u || i == 9u || i == 12u ? 1u : 0u, 1u); }
    return h;
}
static void set_address(bytes& p, std::uint64_t address, unsigned width)
{ put(p, 0u, 1u); uleb(p, 1u + width); put(p, 2u, 1u); put(p, address, width); }
static void end_sequence(bytes& p)
{ put(p, 0u, 1u); uleb(p, 1u); put(p, 1u, 1u); }
static bytes finish_unit(bytes const& body)
{ bytes result; put(result, body.size(), 4u); append(result, body); return result; }
static bytes dwarf4(std::uint64_t begin = 10u)
{
    bytes h{base_header()};
    cstr(h, "src"); cstr(h, "");
    cstr(h, "main.c"); uleb(h, 1u); uleb(h, 0u); uleb(h, 0u); cstr(h, "");
    bytes p; set_address(p, begin, 4u);
    put(p, 1u, 1u); // line 1 at Code offset 10
    put(p, 3u, 1u); sleb(p, 4); // line 5
    put(p, 2u, 1u); uleb(p, 5u); // Code offset 15
    put(p, 1u, 1u);
    put(p, 2u, 1u); uleb(p, 5u); // end at 20, exclusive
    end_sequence(p);
    bytes body; put(body, 4u, 2u); put(body, h.size(), 4u); append(body, h); append(body, p);
    return finish_unit(body);
}
static bytes discriminator_lines(unsigned version)
{
    bytes h{base_header()};
    if(version == 4u)
    {
        cstr(h, "src"); cstr(h, "");
        cstr(h, "main.c"); uleb(h, 1u); uleb(h, 0u); uleb(h, 0u); cstr(h, "");
    }
    else
    {
        // DWARF5 inline string forms, with CU-local directory/file index zero.
        put(h, 1u, 1u); uleb(h, 1u); uleb(h, 8u); uleb(h, 1u); cstr(h, "src");
        put(h, 2u, 1u); uleb(h, 1u); uleb(h, 8u); uleb(h, 2u); uleb(h, 15u);
        uleb(h, 1u); cstr(h, "main.c"); uleb(h, 0u);
    }
    bytes p; set_address(p, 10u, 4u);
    if(version == 5u) { put(p, 4u, 1u); uleb(p, 0u); }
    auto const set_discriminator{[&](std::uint64_t value)
    {
        bytes payload; put(payload, 4u, 1u); uleb(payload, value);
        put(p, 0u, 1u); uleb(p, payload.size()); append(p, payload);
    }};
    set_discriminator(7u); put(p, 1u, 1u); // copy at 10, then reset to zero.
    put(p, 2u, 1u); uleb(p, 5u); // same source line at 15.
    put(p, 5u, 1u); uleb(p, 3u); // persistent source column.
    set_discriminator((std::numeric_limits<std::uint64_t>::max)());
    put(p, 18u, 1u); // special opcode, address/line delta zero; then reset.
    put(p, 2u, 1u); uleb(p, 5u); put(p, 1u, 1u); // copy at 20 has discriminator zero.
    put(p, 2u, 1u); uleb(p, 5u); set_discriminator(9u); end_sequence(p);
    set_address(p, 30u, 4u);
    if(version == 5u) { put(p, 4u, 1u); uleb(p, 0u); }
    put(p, 1u, 1u); // end_sequence reset column, discriminator and line state.
    put(p, 2u, 1u); uleb(p, 5u); end_sequence(p);
    bytes body; put(body, version, 2u);
    if(version == 5u) { put(body, 4u, 1u); put(body, 0u, 1u); }
    put(body, h.size(), 4u); append(body, h); append(body, p);
    return finish_unit(body);
}
static bytes dwarf4_with_dead_sequence(std::uint64_t middle_address)
{
    bytes h{base_header()};
    cstr(h, "src"); cstr(h, "");
    cstr(h, "main.c"); uleb(h, 1u); uleb(h, 0u); uleb(h, 0u); cstr(h, "");
    bytes p;
    set_address(p, 10u, 4u);
    put(p, 1u, 1u); // Tentative row; an ensuing tombstone drops this sequence.
    set_address(p, middle_address, 4u);
    put(p, 1u, 1u);
    put(p, 2u, 1u); uleb(p, 5u);
    end_sequence(p);
    set_address(p, 25u, 4u);
    put(p, 1u, 1u);
    put(p, 2u, 1u); uleb(p, 5u);
    end_sequence(p);
    bytes body; put(body, 4u, 2u); put(body, h.size(), 4u); append(body, h); append(body, p);
    return finish_unit(body);
}
static bytes dwarf5(bytes& strings)
{
    cstr(strings, "main.cpp");
    auto const other_offset{strings.size()};
    cstr(strings, "other.rs");
    bytes h{base_header()};
    put(h, 1u, 1u); uleb(h, 1u); uleb(h, 0x08u); // directory path/string
    uleb(h, 1u); cstr(h, "/src");
    put(h, 2u, 1u); // file entry: path/line_strp, directory/udata
    uleb(h, 1u); uleb(h, 0x1fu);
    uleb(h, 2u); uleb(h, 0x0fu);
    uleb(h, 2u); put(h, 0u, 4u); uleb(h, 0u);
    put(h, other_offset, 4u); uleb(h, 0u);
    bytes p; set_address(p, 30u, 4u);
    put(p, 4u, 1u); uleb(p, 0u); // v5 file 0
    put(p, 5u, 1u); uleb(p, 3u); // column 3
    put(p, 3u, 1u); sleb(p, 6); // line 7
    put(p, 1u, 1u);
    put(p, 2u, 1u); uleb(p, 4u);
    put(p, 4u, 1u); uleb(p, 1u);
    put(p, 3u, 1u); sleb(p, 1);
    put(p, 1u, 1u);
    put(p, 2u, 1u); uleb(p, 6u);
    end_sequence(p);
    bytes body; put(body, 5u, 2u); put(body, 4u, 1u); put(body, 0u, 1u);
    put(body, h.size(), 4u); append(body, h); append(body, p);
    return finish_unit(body);
}
static bytes dwarf64(bytes& strings)
{
    cstr(strings, "wide.c");
    bytes h{base_header()};
    put(h, 1u, 1u); uleb(h, 1u); uleb(h, 0x08u);
    uleb(h, 1u); cstr(h, ""); // DWARF5 compilation directory may be empty
    put(h, 2u, 1u); uleb(h, 1u); uleb(h, 0x1fu);
    uleb(h, 2u); uleb(h, 0x0fu);
    uleb(h, 2u); put(h, 0u, 8u); uleb(h, 0u);
    put(h, 0u, 8u); uleb(h, 0u);
    bytes p; set_address(p, 60u, 8u);
    put(p, 4u, 1u); uleb(p, 0u);
    put(p, 1u, 1u);
    put(p, 2u, 1u); uleb(p, 3u);
    end_sequence(p);
    bytes body; put(body, 5u, 2u); put(body, 8u, 1u); put(body, 0u, 1u);
    put(body, h.size(), 8u); append(body, h); append(body, p);
    bytes unit; put(unit, 0xffffffffu, 4u); put(unit, body.size(), 8u); append(unit, body);
    return unit;
}
static std::span<std::byte const> view(bytes const& data) { return data; }
static void check_synthetic()
{
    source_map map;
    bytes old{dwarf4()};
    check(source_map::parse(8u, {view(old), {}, {}, 40u}, map) == source_map_error::none, "DWARF4 parse");
    check(map.range_count() == 2u, "DWARF4 ranges");
    auto first{map.lookup(8u, 10u)};
    check(first && first->file == "src/main.c" && first->line == 1u, "DWARF4 first location");
    check(map.lookup(8u, 14u)->line == 1u, "DWARF4 interval");
    check(map.lookup(8u, 15u)->line == 5u, "DWARF4 second location");
    check(!map.lookup(8u, 20u) && !map.lookup(9u, 15u), "sequence end and module isolation");
    for(unsigned version : {4u, 5u})
    {
        auto const same_line{discriminator_lines(version)};
        check(source_map::parse(8u, {view(same_line), {}, {}, 40u}, map) == source_map_error::none,
              "DWARF4/5 transient discriminator parse");
        check(map.range_count() == 4u && map.lookup(8u, 10u)->discriminator == 7u &&
              map.lookup(8u, 14u)->discriminator == 7u, "copy discriminator belongs to its entire half-open row");
        check(map.lookup(8u, 15u)->discriminator == (std::numeric_limits<std::uint64_t>::max)() &&
              map.lookup(8u, 15u)->column == 3u && map.lookup(8u, 15u)->line == 1u,
              "special row preserves full discriminator and same-line source column");
        check(map.lookup(8u, 20u)->discriminator == 0u && map.lookup(8u, 20u)->column == 3u,
              "special-row reset preserves persistent column");
        check(map.lookup(8u, 30u)->discriminator == 0u && map.lookup(8u, 30u)->column == 0u &&
              map.lookup(8u, 30u)->is_statement && !map.lookup(8u, 25u),
              "end-sequence resets transient state and leaves its gap unmapped");
    }
    bytes dead{dwarf4_with_dead_sequence(0xffffffffu)};
    check(source_map::parse(8u, {view(dead), {}, {}, 40u}, map) == source_map_error::none,
        "lld dead-function line tombstone accepted");
    check(map.range_count() == 1u && !map.lookup(8u, 10u) && !map.lookup(8u, 15u) &&
          map.lookup(8u, 25u).has_value() && map.lookup(8u, 25u)->line == 1u &&
          !map.lookup(8u, 30u),
        "dead sequence and preceding tentative row are never published");
    bytes out_of_bounds{dwarf4_with_dead_sequence(41u)};
    check(source_map::parse(8u, {view(out_of_bounds), {}, {}, 40u}, map) ==
          source_map_error::malformed, "ordinary out-of-Code address remains rejected");
    check(map.lookup(8u, 25u).has_value(), "malformed address leaves old map unchanged");
    bytes str;
    bytes modern{dwarf5(str)};
    check(source_map::parse(7u, {view(modern), view(str), {}, 50u}, map) == source_map_error::none,
        "DWARF5 line_strp parse");
    first = map.lookup(7u, 30u);
    check(first && first->file == "/src/main.cpp" && first->line == 7u && first->column == 3u,
        "DWARF5 file zero");
    first = map.lookup(7u, 34u);
    check(first && first->file == "/src/other.rs" && first->line == 8u, "DWARF5 file one");
    check(!map.lookup(7u, 40u), "DWARF5 terminal row excluded");

    bytes wide_strings;
    bytes wide{dwarf64(wide_strings)};
    check(source_map::parse(11u, {view(wide), view(wide_strings), {}, 80u}, map) ==
        source_map_error::none, "DWARF64 line table parse");
    first = map.lookup(11u, 60u);
    check(first && first->file == "wide.c" && first->line == 1u,
        "DWARF64 offset/address widths and empty directory");
    check(!map.lookup(11u, 63u), "DWARF64 terminal boundary");

    bytes overlap{old}; append(overlap, old);
    check(source_map::parse(8u, {view(overlap), {}, {}, 40u}, map) ==
        source_map_error::ambiguous_ranges, "overlapping compilation units rejected");
    bytes bad_path{str}; bad_path[0] = std::byte{0x1b};
    check(source_map::parse(7u, {view(modern), view(bad_path), {}, 50u}, map) ==
        source_map_error::malformed, "control byte in source path rejected");

    bytes bad{modern}; bad.pop_back(); // truncated end_sequence
    check(source_map::parse(99u, {view(bad), view(str), {}, 50u}, map) == source_map_error::malformed,
        "truncated unit rejected");
    check(map.lookup(11u, 60u).has_value(), "parse failure leaves old map unchanged");
    auto restrictive{source_map_limits{}}; restrictive.max_rows = 2u;
    check(source_map::parse(7u, {view(modern), view(str), {}, 50u}, map, restrictive) ==
        source_map_error::limit_exceeded, "row limit enforced");
    check(source_map::parse(7u, {view(modern), view(str), {}, 35u}, map) ==
        source_map_error::malformed, "Code section bound enforced");
    restrictive = {}; restrictive.max_section_bytes = 1u;
    check(source_map::parse(7u, {view(modern), view(str), {}, 50u}, map, restrictive) ==
        source_map_error::limit_exceeded, "section byte limit enforced");
    check(source_map::parse(7u, {{}, {}, {}, 50u}, map) ==
        source_map_error::missing_debug_line, "missing -g reported");
    for(std::size_t seed{}; seed != 512u; ++seed)
    {
        bytes mutated{modern};
        auto const at{(seed * 131u) % mutated.size()};
        mutated[at] ^= std::byte{std::uint8_t(1u << (seed % 8u))};
        source_map candidate;
        auto const status{source_map::parse(7u, {view(mutated), view(str), {}, 50u}, candidate)};
        if(status == source_map_error::none)
        {
            for(std::uint64_t offset{}; offset != 50u; ++offset)
            {
                auto const location{candidate.lookup(7u, offset)};
                if(location) { check(!location->file.empty(), "mutated source path is owned"); }
            }
        }
    }
}

static std::uint64_t wasm_uleb(bytes const& data, std::size_t& at, std::size_t end)
{
    std::uint64_t result{};
    if(at >= end || end > data.size()) { fail("truncated Wasm section length"); }
    // [safe data bytes ... at ... end] unsafe (one-past)
    //                      ^^ Both pointers are formed only after bounded offsets.
    auto const first{reinterpret_cast<char const*>(data.data() + at)};
    auto const last{reinterpret_cast<char const*>(data.data() + end)};
    auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::leb128_get(result))};
    if(parsed.code != ::fast_io::parse_code::ok) { fail("invalid Wasm section length LEB"); }
    // [consumed LEB bytes] [remaining section bytes] unsafe (one-past)
    //                    ^^ fast_io returns the next pointer within [first,last].
    at += static_cast<std::size_t>(parsed.iter - first);
    return result;
}
static void check_wasm(char const* path, std::string_view expected_file)
{
    std::ifstream input(path, std::ios::binary);
    check(input.good(), "cannot read -g Wasm fixture");
    std::vector<char> raw{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    bytes data; data.reserve(raw.size());
    for(char c : raw) { data.push_back(std::byte(static_cast<unsigned char>(c))); }
    check(data.size() >= 8u && data[0] == std::byte{} && data[1] == std::byte{'a'} &&
          data[2] == std::byte{'s'} && data[3] == std::byte{'m'}, "bad Wasm fixture magic");
    source_map_sections sections{};
    std::size_t at{8u};
    while(at != data.size())
    {
        auto const id{std::to_integer<std::uint8_t>(data[at++])};
        auto const size{wasm_uleb(data, at, data.size())};
        check(size <= data.size() - at, "bad Wasm section size");
        auto const end{at + static_cast<std::size_t>(size)};
        if(id == 10u) { sections.code_section_content_size = size; }
        if(id == 0u)
        {
            auto const name_size{wasm_uleb(data, at, end)};
            check(name_size <= end - at, "bad custom section name");
            std::string_view name{reinterpret_cast<char const*>(data.data() + at),
                static_cast<std::size_t>(name_size)};
            at += static_cast<std::size_t>(name_size);
            auto const payload{std::span<std::byte const>(data).subspan(at, end - at)};
            if(name == ".debug_line") { sections.debug_line = payload; }
            if(name == ".debug_line_str") { sections.debug_line_str = payload; }
            if(name == ".debug_str") { sections.debug_str = payload; }
        }
        at = end;
    }
    check(sections.code_section_content_size != 0u, "Wasm fixture missing Code section");
    source_map map;
    auto const error{source_map::parse(42u, sections, map)};
    if(error != source_map_error::none)
    {
        ::fast_io::io::perrln("debug_source_map: real Wasm parse error ",
                              ::fast_io::mnp::dec(static_cast<unsigned>(error)));
        fail("real -g Wasm line map");
    }
    check(map.range_count() > 0u, "real -g Wasm has no ranges");
    bool found{};
    for(std::uint64_t offset{}; offset != sections.code_section_content_size; ++offset)
    {
        auto const location{map.lookup(42u, offset)};
        if(location && location->file.find(expected_file) != std::string_view::npos && location->line > 0u)
        { found = true; break; }
    }
    check(found, "real -g Wasm filename/line not found");
    ::fast_io::io::println("PASS real Wasm ", ::fast_io::mnp::os_c_str(path),
                           ": ", ::fast_io::mnp::dec(map.range_count()), " source ranges");
}
int main(int argc, char** argv)
{
    // CU compilation directories are per line-unit, never process cwd.
    for(unsigned version : {4u, 5u})
    {
        auto lines{discriminator_lines(version)};
        source_line_compilation_directory directory{0u, "/build/project"};
        source_map_sections sections{lines, {}, {}, 100u};
        sections.compilation_directories = {&directory, 1u};
        source_map map;
        check(source_map::parse(1u, sections, map) == source_map_error::none &&
              map.lookup(1u, 10u)->file == "/build/project/src/main.c", "DWARF4/5 CU-relative path");
        directory.line_unit_offset = 1u;
        check(source_map::parse(1u, sections, map) == source_map_error::malformed &&
              map.lookup(1u, 10u)->file == "/build/project/src/main.c", "bad line-unit association is atomic");
        directory.line_unit_offset = 0u;
        directory.directory = std::string_view{"bad\0path", 8u};
        check(source_map::parse(1u, sections, map) == source_map_error::malformed, "embedded NUL directory rejected");
    }
    {
        auto first{dwarf4()}; auto lines{first}; append(lines, dwarf4(50u));
        source_line_compilation_directory directories[]{{0u, "/one"}, {first.size(), "/two"}};
        source_map_sections sections{lines, {}, {}, 100u};
        sections.compilation_directories = directories;
        source_map map;
        check(source_map::parse(1u, sections, map) == source_map_error::none &&
              map.lookup(1u, 10u)->file == "/one/src/main.c" &&
              map.lookup(1u, 50u)->file == "/two/src/main.c", "independent CU directories never bleed");
        directories[1u].line_unit_offset = 0u;
        check(source_map::parse(1u, sections, map) == source_map_error::malformed, "duplicate line-unit owner denied");
        directories[1u].line_unit_offset = first.size();
        std::string huge(4097u, 'x'); directories[1u].directory = huge;
        check(source_map::parse(1u, sections, map) == source_map_error::limit_exceeded, "CU directory path budget");
    }
    check_leb_edges();
    check_synthetic();
    if(argc == 3) { check_wasm(argv[1], argv[2]); }
    else { check(argc == 1, "usage: debug_source_map [module.wasm expected-file]"); }
    ::fast_io::io::println("PASS bounded DWARF4/5 source map");
}

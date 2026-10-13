// Core 3 diagnostics use the declared type, not the erased execution carrier.
// Covers recursive GC, typed references, memory64/table64, imported/local tags
// and bounded DWARF summaries. Run remotely with ASan/UBSan as well as release.
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <uwvm2/uwvm/wasm/feature/impl.h>

namespace feature = uwvm2::uwvm::wasm::feature;
namespace w1 = uwvm2::parser::wasm::standard::wasm1::features;
namespace w11 = uwvm2::parser::wasm::standard::wasm1p1::features;
namespace w3 = uwvm2::parser::wasm::standard::wasm3::type;
namespace operation = uwvm2::parser::wasm::concepts::operation;
using bytes = std::vector<std::byte>;
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { fast_io::io::perrln("Core 3 section details: line ", fast_io::mnp::dec(__LINE__), ": ", #value); fast_io::fast_terminate(); } } while(false)

bytes raw(std::initializer_list<unsigned> data)
{ bytes result; for(auto byte : data) { result.push_back(static_cast<std::byte>(byte)); } return result; }
void append_section(bytes& module, unsigned id, bytes const& payload)
{
    module.push_back(static_cast<std::byte>(id));
    auto const size{fast_io::u8concat_fast_io(fast_io::mnp::leb128_put(static_cast<std::uint_least32_t>(payload.size())))};
    for(auto ch : size) { module.push_back(static_cast<std::byte>(ch)); }
    module.insert(module.end(), payload.begin(), payload.end());
}
void append_custom(bytes& module, uwvm2::utils::container::u8string_view name, bytes const& payload)
{
    bytes contents;
    auto const size{fast_io::u8concat_fast_io(fast_io::mnp::leb128_put(static_cast<std::uint_least32_t>(name.size())))};
    for(auto ch : size) { contents.push_back(static_cast<std::byte>(ch)); }
    for(auto ch : name) { contents.push_back(static_cast<std::byte>(ch)); }
    contents.insert(contents.end(), payload.begin(), payload.end());
    append_section(module, 0u, contents);
}
template<typename String>
bool contains(String const& text, uwvm2::utils::container::u8string_view expected)
{
    if(text.size() < expected.size()) { return false; }
    for(std::size_t i{}; i <= text.size() - expected.size(); ++i)
    {
        bool equal{true};
        for(std::size_t j{}; j != expected.size(); ++j) { if(text[i+j] != expected[j]) { equal = false; break; } }
        if(equal) { return true; }
    }
    return false;
}
template<typename Details>
void all_chars(Details details)
{
    fast_io::black_hole c{}; fast_io::wblack_hole w{}; fast_io::u8black_hole u8{};
    fast_io::u16black_hole u16{}; fast_io::u32black_hole u32{};
    fast_io::io::print(c, details); fast_io::io::print(w, details); fast_io::io::print(u8, details);
    fast_io::io::print(u16, details); fast_io::io::print(u32, details);
}
template<typename Section>
auto const& section(auto const& module) { return operation::get_first_type_in_tuple<Section>(module.sections); }

int main()
{
    auto module_bytes{raw({0,0x61,0x73,0x6d,1,0,0,0})};
    append_section(module_bytes, 1u, raw({3,
        0x4e,2,0x5f,2,0x78,1,0x63,0,0,0x5e,0x77,1,
        0x60,2,0x63,0,0x7b,1,0x64,1,
        0x60,1,0x63,0,0}));
    append_section(module_bytes, 2u, raw({5,
        4,'h','o','s','t',1,'e',4,0,3,
        4,'h','o','s','t',1,'t',1,0x63,2,5,0x80,0x80,0x80,0x80,0x10,0x81,0x80,0x80,0x80,0x10,
        4,'h','o','s','t',1,'m',2,5,0x80,0x80,0x80,0x80,0x10,0x81,0x80,0x80,0x80,0x10,
        4,'h','o','s','t',1,'g',3,0x63,0,1,
        4,'h','o','s','t',1,'n',3,0x64,0,0}));
    append_section(module_bytes, 4u, raw({1,0x63,2,5,0x80,0x80,0x80,0x80,0x10,0x81,0x80,0x80,0x80,0x10}));
    append_section(module_bytes, 5u, raw({1,5,0x80,0x80,0x80,0x80,0x10,0x81,0x80,0x80,0x80,0x10}));
    append_section(module_bytes, 13u, raw({2,0,3,0,3}));
    append_section(module_bytes, 6u, raw({1,0x63,0,0,0xd0,0,0x0b}));
    append_section(module_bytes, 9u, raw({1,5,0x63,0,1,0xd0,0,0x0b}));
    auto const dwarf32{raw({9,0,0,0,5,0,1,4,0,0,0,0,0})};
    append_custom(module_bytes, u8".debug_info", dwarf32);
    append_custom(module_bytes, u8".debug_line", raw({1,2,3})); // opaque custom data must not invalidate Wasm
    append_custom(module_bytes, u8".debug_abbrev", raw({0}));
    append_custom(module_bytes, uwvm2::utils::container::u8string_view{u8"unsafe\n)\\\0name",14uz}, raw({7}));

    feature::wasm_binfmt_ver1_feature_parameter_storage_t parameters{};
    auto& policy{feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters)};
    policy.disable_reference_types = false; policy.disable_simd = false; policy.disable_multi_value = false;
    policy.disable_gc = false; policy.disable_function_references = false; policy.disable_exceptions = false;
    policy.disable_memory64 = false; policy.disable_table64 = false;
    policy.disable_multi_memory = false;
    uwvm2::parser::wasm::base::error_impl error{};
    auto const module{feature::binfmt_ver1_handler(module_bytes.data(), module_bytes.data()+module_bytes.size(), error, parameters)};
    CHECK(error.err_code == uwvm2::parser::wasm::base::wasm_parse_error_code::ok);
    using f1 = w1::wasm1; using f11 = w11::wasm1p1;
    auto const& types{section<w1::type_section_storage_t<f1,f11>>(module)};
    auto const type_details{section_details(types, module.sections)};
    auto const type_text{fast_io::u8concat_fast_io(type_details)};
    CHECK(contains(type_text, u8"Type[4]"));
    CHECK(contains(type_text, u8"rec[0]: first-type=0, count=2"));
    CHECK(contains(type_text, u8"type[0]: (sub final (struct (field (mut i8)) (field (ref null 0))))"));
    CHECK(contains(type_text, u8"type[1]: (sub final (array (mut i16)))"));
    CHECK(contains(type_text, u8"type[2]: (sub final (func (param (ref null 0) v128) (result (ref 1))))"));
    CHECK(!contains(type_text, u8"(array (field"));
    all_chars(type_details);
    auto const import_details{section_details(section<w1::import_section_storage_t<f1,f11>>(module), module.sections)};
    auto const import_text{fast_io::u8concat_fast_io(import_details)};
    CHECK(contains(import_text, u8"Import[5]"));
    CHECK(contains(import_text, u8"tag[0]: {3}"));
    CHECK(contains(import_text, u8"table[0]: {type: (ref null 2)"));
    CHECK(contains(import_text, u8"memory[0]: {"));
    CHECK(contains(import_text, u8"4294967296")); CHECK(contains(import_text, u8"4294967297"));
    CHECK(contains(import_text, u8"address64: 1"));
    CHECK(contains(import_text, u8"global[0]: {type: (ref null 0), mutable: 1}"));
    CHECK(contains(import_text, u8"global[1]: {type: (ref 0), mutable: 0}"));
    CHECK(!contains(import_text, u8"global[2]:"));
    all_chars(import_details);
    auto const table_details{section_details(section<w1::table_section_storage_t<f1,f11>>(module), module.sections)};
    auto const table_text{fast_io::u8concat_fast_io(table_details)};
    CHECK(contains(table_text, u8"type: (ref null 2)"));
    CHECK(contains(table_text, u8"4294967296")); CHECK(contains(table_text, u8"4294967297"));
    CHECK(contains(table_text, u8"address64: 1")); all_chars(table_details);
    auto const memory_details{section_details(section<w1::memory_section_storage_t<f1,f11>>(module), module.sections)};
    auto const memory_text{fast_io::u8concat_fast_io(memory_details)};
    CHECK(contains(memory_text, u8"4294967296")); CHECK(contains(memory_text, u8"address64: 1")); all_chars(memory_details);
    auto const tag_details{section_details(section<w11::tag_section_storage_t<f1,f11>>(module), module.sections)};
    auto const tag_text{fast_io::u8concat_fast_io(tag_details)};
    CHECK(contains(tag_text, u8"Tag[2]")); CHECK(contains(tag_text, u8"localtag[0] -> tag[1]: {type: 3}"));
    CHECK(contains(tag_text, u8"localtag[1] -> tag[2]: {type: 3}")); all_chars(tag_details);
    auto const global_details{section_details(section<w1::global_section_storage_t<f1,f11>>(module), module.sections)};
    CHECK(contains(fast_io::u8concat_fast_io(global_details), u8"type: (ref null 0)")); all_chars(global_details);
    auto const element_details{section_details(section<w1::element_section_storage_t<f1,f11>>(module), module.sections)};
    CHECK(contains(fast_io::u8concat_fast_io(element_details), u8"type: (ref null 0)")); all_chars(element_details);
    auto const custom_details{section_details<f1, f11>(section<w1::custom_section_storage_t>(module), module.sections)};
    auto const custom_text{fast_io::u8concat_fast_io(custom_details)};
    CHECK(contains(custom_text, u8"custom (.debug_info): size = 13, content-size = 25"));
    CHECK(contains(custom_text, u8"unit-headers = complete, dwarf32-units = 1, dwarf64-units = 0, versions = 5"));
    CHECK(contains(custom_text, u8"unit-headers = truncated")); CHECK(contains(custom_text, u8"kind = DWARF auxiliary"));
    CHECK(contains(custom_text, u8"unsafe\\x0a\\x29\\x5c\\x00name")); all_chars(custom_details);

    using namespace w3::details::custom_payload;
    for(std::size_t n{}; n != dwarf32.size(); ++n)
    {
        auto const summary{summarize_unit_headers(dwarf32.data(), dwarf32.data()+n)};
        CHECK(summary.units32 == 0uz && summary.units64 == 0uz);
        CHECK(summary.status == (n == 0uz ? header_status::complete : header_status::truncated));
    }
    auto const dwarf64{raw({0xff,0xff,0xff,0xff,13,0,0,0,0,0,0,0,5,0,1,4,0,0,0,0,0,0,0,0,0})};
    auto mixed{dwarf32}; mixed.insert(mixed.end(), dwarf64.begin(), dwarf64.end());
    auto const summary{summarize_unit_headers(mixed.data(), mixed.data()+mixed.size())};
    CHECK(summary.status == header_status::complete && summary.units32 == 1uz && summary.units64 == 1uz);
    auto reserved{dwarf32}; reserved[0] = std::byte{0xf0}; reserved[1] = reserved[2] = reserved[3] = std::byte{0xff};
    CHECK(summarize_unit_headers(reserved.data(), reserved.data()+reserved.size()).status == header_status::reserved_length);
    auto unsupported{dwarf32}; unsupported[4] = std::byte{6};
    CHECK(summarize_unit_headers(unsupported.data(), unsupported.data()+unsupported.size()).status == header_status::unsupported_version);
    auto wrong_kind{dwarf32}; wrong_kind[6] = std::byte{7};
    CHECK(summarize_unit_headers(wrong_kind.data(), wrong_kind.data()+wrong_kind.size()).status == header_status::unsupported_unit_type);
    CHECK(summarize_unit_headers(nullptr,nullptr).status == header_status::complete);
    fast_io::io::println("Core 3 section details checks: ", fast_io::mnp::dec(checks));
}

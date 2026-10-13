/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Include the public DATA header first; instantiate the entire formatter with
// the exact std::string sink used by the management console. No VM authority.
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <bit>
#include <string>
#include <string_view>
#include <fast_io_unit/string.h>
namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
namespace
{
    unsigned checks{};
    bool expect(bool accepted, ::fast_io::string_view label)
    {
        ++checks;
        if(!accepted) { ::fast_io::println(::fast_io::err(), "FAILED WASIp1 formatter: ", label); }
        return accepted;
    }
    bool contains(::std::string const& text, ::std::string_view part) noexcept
    { return ::std::string_view{text}.find(part) != ::std::string_view::npos; }
    ::std::string render(ws::view const& data)
    {
        ::std::string text{};
        ws::print(::fast_io::ostring_ref_std{__builtin_addressof(text)}, data);
        return text;
    }
    bool limited(ws::view const& data)
    { return render(data) == "error: WASIp1 reply resource limit\n"; }
}
int main(int argc, char const** argv)
{
    if(argc == 2 && ::std::string_view{argv[1]} == "--require-big")
    { if(::std::endian::native != ::std::endian::big) { return 90; } }
    else if(argc != 1) { return 91; }
    bool good{true};
    ws::view data{};
    data.result = ws::status::ok; data.module = UINT64_MAX; data.observed_runtime_epoch = 9u;
    auto text{render(data)};
    good &= expect(contains(text, "wasip1 module=18446744073709551615 status=ok epoch=9") &&
        !contains(text, "more next="), "empty reply exact unsigned coordinates/no continuation");
    data.strings.push_back({UINT64_MAX, ws::text{u8"safe\n\x1b[31m\"\\"}});
    text = render(data);
    good &= expect(contains(text, "[18446744073709551615] \"safe\\x0a\\x1b[31m\\x22\\x5c\""),
        "opaque byte text escapes terminal controls, quote and slash");
    for(unsigned i{}; i != 5u; ++i)
    {
        data.descriptors.push_back({static_cast<::std::uint64_t>(i), UINT64_MAX, 0u,
            static_cast<ws::descriptor_kind>(i), i == 2u, ws::text{u8"/guest\n\"\\"}});
    }
    data.more = true; data.next = UINT64_MAX; data.mutation_applied = true; data.shared_environment = true;
    text = render(data);
    good &= expect(contains(text, "storage-kind=file rights-base=0xffffffffffffffff") &&
        contains(text, "storage-kind=file observer") && contains(text, "storage-kind=directory") &&
        contains(text, "storage-kind=socket rights-base=") && contains(text, "storage-kind=socket observer"),
        "all five descriptor storage kinds and full rights bits");
    good &= expect(contains(text, "guest-name=\"/guest\\x0a\\x22\\x5c\"") &&
        contains(text, "more next=18446744073709551615"), "true preopen/continuation branches");
    for(unsigned i{}; i != 17u; ++i)
    {
        data.result = static_cast<ws::status>(i); text = render(data);
        auto const status{ws::status_text(data.result)};
        good &= expect(contains(text, ::std::string_view{status.data(), status.size()}), "all result status formatting");
    }
    data.result = static_cast<ws::status>(255u); data.descriptors[0u].kind = static_cast<ws::descriptor_kind>(255u);
    text = render(data);
    good &= expect(contains(text, "status=invalid result") && contains(text, "storage-kind=invalid kind"),
        "invalid detached enum values format without native access");
    data = {}; data.result = ws::status::ok;
    ws::text hostile{}; hostile.resize(ws::maximum_page_text_bytes, u8'\x1b');
    data.strings.push_back({0u, ::std::move(hostile)}); text = render(data);
    good &= expect(text.size() < 65536u && contains(text, "\\x1b"), "4096 byte hostile page respects broker cap");
    data.strings.push_back({1u, ws::text{u8"a"}});
    good &= expect(limited(data), "cumulative string page overflow rejected before any partial reply");
    data = {}; data.strings.resize(ws::maximum_rows + 1u);
    good &= expect(limited(data), "string row limit");
    data = {}; data.descriptors.resize(ws::maximum_rows + 1u);
    good &= expect(limited(data), "descriptor row limit");
    data = {}; data.strings.resize(ws::maximum_rows); data.descriptors.resize(1u);
    good &= expect(limited(data), "combined row limit");
    data = {}; ws::text too_long{}; too_long.resize(ws::maximum_text_bytes + 1u, u8'x');
    data.strings.push_back({0u, ::std::move(too_long)});
    good &= expect(limited(data), "single string byte limit");
    data = {}; ws::text preopen{}; preopen.resize(ws::maximum_page_text_bytes, u8'\x1b');
    data.descriptors.push_back({3u, 1u, 3u, ws::descriptor_kind::directory, true, ::std::move(preopen)});
    text = render(data);
    good &= expect(text.size() < 65536u && contains(text, "guest-name=\"\\x1b"), "maximum preopen text page");
    data.strings.push_back({1u, ws::text{u8"a"}});
    good &= expect(limited(data), "cumulative mixed string/preopen page limit");
    data = {}; ws::text oversize_preopen{}; oversize_preopen.resize(ws::maximum_text_bytes + 1u, u8'x');
    data.descriptors.push_back({3u, 1u, 3u, ws::descriptor_kind::directory, true, ::std::move(oversize_preopen)});
    good &= expect(limited(data), "single descriptor name byte limit");
    ::fast_io::println(::fast_io::out(), "WASIp1 whole formatter DATA checks=", ::fast_io::mnp::dec(checks),
        " host-endian=", ::std::endian::native == ::std::endian::big ? ::fast_io::string_view{"big"} : ::fast_io::string_view{"little"});
    return good ? 0 : 1;
}

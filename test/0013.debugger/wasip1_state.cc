// Component contract tests. This executable owns NO VM pause/host-gate issuer.
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/utils/container/impl.h>
namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
int main()
{
    ws::request selected{};
    if(!ws::parse("info wasip1 args 0", selected) || selected.operation != ws::action::arguments || selected.count != 64u) { return 1; }
    if(!ws::parse("info wasip1 env 18446744073709551615 5 8", selected) || selected.module != UINT64_MAX || selected.first != 5u || selected.count != 8u) { return 2; }
    if(!ws::parse("set wasip1 arg 0 1 612062", selected) || selected.operation != ws::action::replace_argument || selected.value != u8"a b") { return 3; }
    if(!ws::parse("set wasip1 env 0 484f4d45 2f77617369", selected) || selected.name != u8"HOME" || selected.value != u8"/wasi") { return 4; }
    if(!ws::parse("set wasip1 env 0 484f4d45 -", selected) || !selected.value.empty()) { return 5; }
    if(!ws::parse("unset wasip1 env 0 484f4d45", selected) || selected.operation != ws::action::remove_environment || selected.name != u8"HOME") { return 6; }
    if(!ws::parse("set wasip1 rights 0 3 15 31 1 3", selected) || selected.operation != ws::action::reduce_rights || selected.new_inheriting != 3u) { return 7; }
    if(!ws::rights_subset(1u, 3u, 15u, 31u) || ws::rights_subset(16u, 0u, 15u, 31u) || ws::rights_subset(0u, 32u, 15u, 31u)) { return 8; }
    if(!ws::environment_key_matches(u8"A=two=equals", u8"A") || ws::environment_key_matches(u8"AB=x", u8"A") || ws::environment_key_matches(u8"A", u8"A")) { return 9; }
    ::fast_io::array<::fast_io::string_view, 16u> const rejected{
        "info wasip1 args +1", "info wasip1 args -1", "info wasip1 args 18446744073709551616",
        "info wasip1 args 0 0 0", "info wasip1 args 0 0 65", "info wasip1 native-handles 0",
        "set wasip1 arg 0 4096 61", "set wasip1 arg 0 0 00", "set wasip1 arg 0 0 6",
        "set wasip1 arg 0 0 +f", "set wasip1 arg 0 0 ffZZ", "set wasip1 env 0 41= 62",
        "set wasip1 env 0 413d 62", "set wasip1 env 0 - 62", "unset wasip1 env 0 -",
        "set wasip1 rights 0 2147483648 0 0 0 0"};
    for(auto command : rejected) { if(ws::parse(command, selected)) { return 10; } }
    // Parsing failure is transactional: output request remains unchanged.
    if(!ws::parse("info wasip1 fds 7 3 4", selected)) { return 11; }
    if(ws::parse("set wasip1 arg 0 0 00", selected) || selected.module != 7u || selected.first != 3u) { return 12; }
    ::uwvm2::utils::container::string rendered{};
    ws::text raw{u8"line\n\x1b[31m\"\\"};
    ws::print_escaped(::uwvm2::utils::container::string_ref_uwvm{::std::addressof(rendered)}, ws::text_view{raw.data(), raw.size()});
    if(rendered != "line\\x0a\\x1b[31m\\x22\\x5c") { return 13; }
    ws::view copied{}; copied.result = ws::status::ok; copied.module = 1u; copied.observed_runtime_epoch = 9u;
    copied.strings.push_back({0u, ws::text{u8"name=value"}});
    copied.descriptors.push_back({3u, 1u, 3u, ws::descriptor_kind::directory, true, ws::text{u8"/guest"}});
    rendered.clear(); ws::print(::uwvm2::utils::container::string_ref_uwvm{::std::addressof(rendered)}, copied);
    if(rendered.empty()) { return 14; }
    // Maximum hostile byte text stays within the real broker's reply cap.
    copied = {}; copied.result = ws::status::ok;
    ws::text full{}; full.resize(ws::maximum_page_text_bytes, u8'\x1b');
    copied.strings.push_back({0u, ::std::move(full)});
    rendered.clear(); ws::print(::uwvm2::utils::container::string_ref_uwvm{::std::addressof(rendered)}, copied);
    if(rendered.size() > 65536u) { return 15; }
    copied.strings.push_back({1u, ws::text{u8"over-budget"}});
    rendered.clear(); ws::print(::uwvm2::utils::container::string_ref_uwvm{::std::addressof(rendered)}, copied);
    if(rendered != "error: WASIp1 reply resource limit\n") { return 16; }
    if(!ws::parse("set wasip1 rights 0 3 0xffffffffffffffff 0X1F 0x0001 3", selected) ||
       selected.expected_base != UINT64_MAX || selected.expected_inheriting != 31u || selected.new_base != 1u) { return 17; }
    for(auto line : ::fast_io::array<::fast_io::string_view, 6u>{
        "set wasip1 rights 0 3 0x 0 0 0", "set wasip1 rights 0 3 0x+1 0 0 0", "set wasip1 rights 0 3 0x-1 0 0 0",
        "set wasip1 rights 0 3 0x10000000000000000 0 0 0", "set wasip1 rights 0 3 0x1z 0 0 0", "set wasip1 rights 0x0 3 1 0 0 0"})
    { if(ws::parse(line, selected)) { return 18; } }
    if(!ws::parse("set wasip1 arg-insert 0 2 612062", selected) || selected.operation != ws::action::insert_argument || selected.value != u8"a b") { return 19; }
    if(!ws::parse("unset wasip1 arg 0 2", selected) || selected.operation != ws::action::remove_argument) { return 20; }
    ::fast_io::io::print(::fast_io::out(), "wasip1_state component ok\n");
}

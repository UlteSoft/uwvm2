// Include the production header first: an earlier fast_io umbrella must not
// suppress its standard-string hooks. No test -include repairs dependencies.
#include <uwvm2/uwvm/debugger/source_map.h>
#include <array>
#include <climits>
#include <limits>

#if CHAR_BIT == 8
using reader = ::uwvm2::uwvm::debugger::source_map_details::reader;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); return 1; } } while(false)
int main()
{
    ::std::array<::std::byte, 10u> const wire{::std::byte{0x55u}, ::std::byte{0x88u}, ::std::byte{0x77u},
        ::std::byte{0x66u}, ::std::byte{0x55u}, ::std::byte{0x44u}, ::std::byte{0x33u}, ::std::byte{0x22u},
        ::std::byte{0x91u}, ::std::byte{0xAAu}};
    ::std::array<::std::uint64_t, 8u> const expected{0x88uLL, 0x7788uLL, 0x667788uLL, 0x55667788uLL,
        0x4455667788uLL, 0x334455667788uLL, 0x22334455667788uLL, 0x9122334455667788uLL};
    ::std::span<::std::byte const> const complete{wire};
    for(::std::size_t n{1u}; n <= expected.size(); ++n)
    {
        reader input{complete, 1u};
        ::std::uint64_t value{0xA5A5A5A5A5A5A5A5uLL};
        CHECK(input.fixed(n, value) && value == expected[n - 1u] && input.cursor == n + 1u);
        CHECK(wire.front() == ::std::byte{0x55u} && wire.back() == ::std::byte{0xAAu});
        // n <= 8 < complete.size() is established by the loop. The truncated
        // extent contains n-1 payload bytes, so neither output nor cursor may
        // be published and no one-past source may be dereferenced.
        reader truncated{complete.first(n), 1u};
        value = 0xA5A5A5A5A5A5A5A5uLL;
        CHECK(!truncated.fixed(n, value) && value == 0xA5A5A5A5A5A5A5A5uLL && truncated.cursor == 1u);
    }
    for(auto const invalid_width : ::std::array{0u, 9u})
    {
        reader input{complete, 1u};
        ::std::uint64_t value{0xA5A5A5A5A5A5A5A5uLL};
        CHECK(!input.fixed(invalid_width, value) && value == 0xA5A5A5A5A5A5A5A5uLL && input.cursor == 1u);
    }
    for(auto const cursor : ::std::array{complete.size() + 1u, (::std::numeric_limits<::std::size_t>::max)()})
    {
        reader input{complete, cursor};
        ::std::uint64_t value{0xA5A5A5A5A5A5A5A5uLL};
        CHECK(!input.fixed(1u, value) && value == 0xA5A5A5A5A5A5A5A5uLL && input.cursor == cursor);
        auto span_output{complete.first(1u)};
        auto const output_data{span_output.data()};
        auto const output_size{span_output.size()};
        CHECK(!input.take(0u, span_output) && input.cursor == cursor && span_output.data() == output_data && span_output.size() == output_size);
        CHECK(!input.skip(0u) && input.cursor == cursor);
        ::std::uint8_t byte_output{0xA5u};
        CHECK(!input.byte(byte_output) && input.cursor == cursor && byte_output == 0xA5u);
        ::std::string_view text{"untouched"};
        CHECK(!input.cstring(text, 32u) && input.cursor == cursor && text == "untouched");
    }
    reader boundary{complete, complete.size()};
    ::std::span<::std::byte const> span_output{complete};
    CHECK(boundary.take(0u, span_output) && span_output.empty() && boundary.cursor == complete.size());
    CHECK(boundary.skip(0u) && boundary.cursor == complete.size());
    ::std::uint8_t byte_output{0xA5u};
    CHECK(!boundary.byte(byte_output) && byte_output == 0xA5u && boundary.cursor == complete.size());
    ::std::uint64_t value{0xA5A5A5A5A5A5A5A5uLL};
    CHECK(!boundary.fixed(1u, value) && value == 0xA5A5A5A5A5A5A5A5uLL && boundary.cursor == complete.size());
    reader empty{};
    CHECK(!empty.fixed(1u, value) && value == 0xA5A5A5A5A5A5A5A5uLL && empty.cursor == 0u);
    CHECK(!empty.byte(byte_output) && byte_output == 0xA5u && empty.cursor == 0u);
    reader single{complete, 1u};
    CHECK(single.byte(byte_output) && byte_output == 0x88u && single.cursor == 2u);
    auto const copied_path{::uwvm2::uwvm::debugger::source_map_details::absolute_path("C:/module/main.cpp")};
    CHECK(copied_path);
    ::fast_io::println("PASS source-map binary reader checks ", checks);
}
#else
int main() { return 77; }
#endif

#include <cstdio>
#include <cstdlib>

#include <uwvm2/utils/container/string_concat.h>

namespace uwvm_test_concat_forwarding
{
    struct stateful_value
    {
        unsigned* calls;
    };

    // Deliberately return the same printable type: an extra normalization pass
    // remains well-formed but becomes observable through the call count.
    template <::std::integral Char>
    stateful_value status_io_print_forward(::fast_io::io_alias_type_t<Char>, stateful_value value) noexcept
    {
        ++*value.calls;
        return value;
    }

    template <::std::integral Char>
    constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<Char, stateful_value>) noexcept
    { return 1; }

    template <::std::integral Char>
    Char* print_reserve_define(::fast_io::io_reserve_type_t<Char, stateful_value>, Char* out, stateful_value) noexcept
    {
        *out = static_cast<Char>('X');
        return out + 1;
    }

    struct category_value
    {
    };

    unsigned print_alias_define(::fast_io::io_alias_t, category_value&) noexcept { return 1; }

    unsigned print_alias_define(::fast_io::io_alias_t, category_value&&) noexcept { return 2; }

    struct lifetime_state
    {
        unsigned live{};
        unsigned forwards{};
        unsigned prints{};
    };

    struct lifetime_source
    {
        lifetime_state* state;
    };

    template <::std::integral Char>
    struct owning_proxy
    {
        lifetime_state* state;
        Char* storage;

        explicit owning_proxy(lifetime_state* value) : state{value}, storage{new Char[1]{static_cast<Char>('P')}} { ++state->live; }

        owning_proxy(owning_proxy const&) = delete;

        owning_proxy(owning_proxy&& other) noexcept : state{other.state}, storage{other.storage} { other.storage = nullptr; }

        ~owning_proxy()
        {
            if(storage != nullptr)
            {
                --state->live;
                delete[] storage;
            }
        }
    };

    // Only the rvalue overload exists. Its move-only result must remain alive
    // throughout reserve/define, then be destroyed before concat returns.
    template <::std::integral Char>
    owning_proxy<Char> status_io_print_forward(::fast_io::io_alias_type_t<Char>, lifetime_source&& source)
    {
        ++source.state->forwards;
        return owning_proxy<Char>{source.state};
    }

    template <::std::integral Char>
    constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<Char, owning_proxy<Char>>) noexcept
    { return 1; }

    template <::std::integral Char>
    Char* print_reserve_define(::fast_io::io_reserve_type_t<Char, owning_proxy<Char>>, Char* out, owning_proxy<Char> const& value) noexcept
    {
        if(value.state->live != 1 || value.storage == nullptr) { ::std::abort(); }
        ++value.state->prints;
        *out = value.storage[0];
        return out + 1;
    }

    struct destination_value
    {
    };

    // This customization accepts the real string output adapter, not the dummy
    // stream formerly used by uwvm's separate printability check.
    template <::std::integral Char, typename Allocator>
    void print_define(::fast_io::io_reserve_type_t<Char, destination_value>,
                      ::fast_io::io_strlike_reference_wrapper<Char, ::fast_io::containers::basic_string<Char, Allocator>> stream,
                      destination_value)
    {
        Char const text[]{static_cast<Char>('D')};
        ::fast_io::operations::write_all(stream, text, text + 1);
    }

    template <bool Line, typename String>
    bool equals_character(String const& text, char expected)
    { return text.size() == (Line ? 2uz : 1uz) && text[0] == expected && (!Line || text[1] == '\n'); }

    template <bool Line, typename Concat>
    bool check_concat(Concat concat, char const* name)
    {
        unsigned calls{};
        auto stateful{concat(stateful_value{&calls})};
        category_value lvalue{};
        lifetime_state lifetime{};
        auto owned{concat(lifetime_source{&lifetime})};
        bool const passed{calls == 1 && equals_character<Line>(stateful, 'X') && lifetime.live == 0 && lifetime.forwards == 1 && lifetime.prints == 1 &&
                          equals_character<Line>(owned, 'P') && equals_character<Line>(concat(lvalue), '1') &&
                          equals_character<Line>(concat(category_value{}), '2') && equals_character<Line>(concat(destination_value{}), 'D') &&
                          concat().size() == (Line ? 1uz : 0uz)};
        if(!passed) { ::std::fprintf(stderr, "%s: normalization/category/destination regression (calls=%u)\n", name, calls); }
        return passed;
    }
}  // namespace uwvm_test_concat_forwarding

#define UWVM_TEST_CONCAT(line, function)                                                                                                                       \
    if(!::uwvm_test_concat_forwarding::check_concat<line>([]<typename... Args>(Args&&... args) { return function(static_cast<Args&&>(args)...); }, #function)) \
    {                                                                                                                                                          \
        return 1;                                                                                                                                              \
    }

int main()
{
    using namespace ::uwvm2::utils::container;
    UWVM_TEST_CONCAT(false, basic_concat_uwvm<char>)
    UWVM_TEST_CONCAT(false, basic_concat_uwvm<wchar_t>)
    UWVM_TEST_CONCAT(false, basic_concat_uwvm<char8_t>)
    UWVM_TEST_CONCAT(false, basic_concat_uwvm<char16_t>)
    UWVM_TEST_CONCAT(false, basic_concat_uwvm<char32_t>)
    UWVM_TEST_CONCAT(false, concat_uwvm)
    UWVM_TEST_CONCAT(false, wconcat_uwvm)
    UWVM_TEST_CONCAT(false, u8concat_uwvm)
    UWVM_TEST_CONCAT(false, u16concat_uwvm)
    UWVM_TEST_CONCAT(false, u32concat_uwvm)
    UWVM_TEST_CONCAT(true, concatln_uwvm)
    UWVM_TEST_CONCAT(true, wconcatln_uwvm)
    UWVM_TEST_CONCAT(true, u8concatln_uwvm)
    UWVM_TEST_CONCAT(true, u16concatln_uwvm)
    UWVM_TEST_CONCAT(true, u32concatln_uwvm)
    UWVM_TEST_CONCAT(false, tlc::basic_concat_uwvm_tlc<char>)
    UWVM_TEST_CONCAT(false, tlc::basic_concat_uwvm_tlc<wchar_t>)
    UWVM_TEST_CONCAT(false, tlc::basic_concat_uwvm_tlc<char8_t>)
    UWVM_TEST_CONCAT(false, tlc::basic_concat_uwvm_tlc<char16_t>)
    UWVM_TEST_CONCAT(false, tlc::basic_concat_uwvm_tlc<char32_t>)
    UWVM_TEST_CONCAT(false, tlc::concat_uwvm_tlc)
    UWVM_TEST_CONCAT(false, tlc::wconcat_uwvm_tlc)
    UWVM_TEST_CONCAT(false, tlc::u8concat_uwvm_tlc)
    UWVM_TEST_CONCAT(false, tlc::u16concat_uwvm_tlc)
    UWVM_TEST_CONCAT(false, tlc::u32concat_uwvm_tlc)
    UWVM_TEST_CONCAT(true, tlc::concatln_uwvm_tlc)
    UWVM_TEST_CONCAT(true, tlc::wconcatln_uwvm_tlc)
    UWVM_TEST_CONCAT(true, tlc::u8concatln_uwvm_tlc)
    UWVM_TEST_CONCAT(true, tlc::u16concatln_uwvm_tlc)
    UWVM_TEST_CONCAT(true, tlc::u32concatln_uwvm_tlc)
}

#undef UWVM_TEST_CONCAT

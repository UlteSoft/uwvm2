/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <concepts>

#ifndef UWVM_MODULE
# include <fast_io.h>
#else
# error "Module testing is not currently supported"
#endif

#define UWVM_TEST_REQUIRE(expr) \
    do \
    { \
        if(!(expr)) \
        { \
            return __LINE__; \
        } \
    } while(false)

namespace uwvm2_test_scan_precise_batch
{
    struct scan_input_state
    {
        char* begin{};
        char* curr{};
        char* end{};
        char const* cold_curr{};
        char const* cold_end{};
        ::std::size_t begin_calls{};
        ::std::size_t curr_calls{};
        ::std::size_t end_calls{};
        ::std::size_t set_calls{};
        ::std::size_t underflow_calls{};
        ::std::size_t read_all_calls{};
    };

    struct scan_input_ref
    {
        using input_char_type = char;
        scan_input_state* state{};
    };

    inline char* ibuffer_begin(scan_input_ref in) noexcept
    {
        ++in.state->begin_calls;
        return in.state->begin;
    }

    inline char* ibuffer_curr(scan_input_ref in) noexcept
    {
        ++in.state->curr_calls;
        return in.state->curr;
    }

    inline char* ibuffer_end(scan_input_ref in) noexcept
    {
        ++in.state->end_calls;
        return in.state->end;
    }

    inline void ibuffer_set_curr(scan_input_ref in, char* ptr) noexcept
    {
        ++in.state->set_calls;
        in.state->curr = ptr;
    }

    inline bool ibuffer_underflow(scan_input_ref in) noexcept
    {
        ++in.state->underflow_calls;
        return false;
    }

    inline void read_all_underflow_define(scan_input_ref in, char* first, char* last) noexcept
    {
        ++in.state->read_all_calls;
        auto const n{static_cast<::std::size_t>(last - first)};
        if(static_cast<::std::size_t>(in.state->cold_end - in.state->cold_curr) < n)
        {
            ::std::abort();
        }
        for(::std::size_t i{}; i != n; ++i)
        {
            first[i] = in.state->cold_curr[i];
        }
        in.state->cold_curr += n;
    }

    inline ::std::uint64_t load_le(char const* p, ::std::size_t n) noexcept
    {
        ::std::uint64_t value{};
        for(::std::size_t i{}; i != n; ++i)
        {
            value |= static_cast<::std::uint64_t>(static_cast<unsigned char>(p[i])) << (i * 8u);
        }
        return value;
    }

    struct get_u8
    {
        ::std::uint64_t* out{};
    };

    struct get_u16
    {
        ::std::uint64_t* out{};
    };

    struct get_u32
    {
        ::std::uint64_t* out{};
    };

    struct get_u64
    {
        ::std::uint64_t* out{};
    };

    struct get_u16_may_error
    {
        ::std::uint64_t* out{};
    };

    struct get_u16_end_of_file
    {
        ::std::uint64_t* out{};
    };

    struct context3
    {
        ::std::uint64_t* out{};
    };

    struct context3_state
    {};

    template <::std::integral char_type>
    struct unit_input_state
    {
        char_type* begin{};
        char_type* curr{};
        char_type* end{};
        ::std::size_t curr_calls{};
        ::std::size_t end_calls{};
        ::std::size_t set_calls{};
    };

    template <::std::integral char_type>
    struct unit_input_ref
    {
        using input_char_type = char_type;
        unit_input_state<char_type>* state{};
    };

    template <::std::integral char_type>
    inline char_type* ibuffer_begin(unit_input_ref<char_type> in) noexcept
    {
        return in.state->begin;
    }

    template <::std::integral char_type>
    inline char_type* ibuffer_curr(unit_input_ref<char_type> in) noexcept
    {
        ++in.state->curr_calls;
        return in.state->curr;
    }

    template <::std::integral char_type>
    inline char_type* ibuffer_end(unit_input_ref<char_type> in) noexcept
    {
        ++in.state->end_calls;
        return in.state->end;
    }

    template <::std::integral char_type>
    inline void ibuffer_set_curr(unit_input_ref<char_type> in, char_type* ptr) noexcept
    {
        ++in.state->set_calls;
        in.state->curr = ptr;
    }

    template <::std::integral char_type>
    inline bool ibuffer_underflow(unit_input_ref<char_type>) noexcept
    {
        return false;
    }

    template <::std::integral char_type, ::std::size_t n>
    struct unit_get
    {
        ::std::uint64_t* out{};
    };

template <::std::integral char_type>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, get_u8>) noexcept
{
    return 1u;
}

template <::std::integral char_type>
inline constexpr void scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, get_u8>, char_type const* p, get_u8 out) noexcept
{
    *out.out = load_le(p, 1u);
}

template <::std::integral char_type>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, get_u16>) noexcept
{
    return 2u;
}

template <::std::integral char_type>
inline constexpr void scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, get_u16>, char_type const* p, get_u16 out) noexcept
{
    *out.out = load_le(p, 2u);
}

template <::std::integral char_type>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, get_u32>) noexcept
{
    return 4u;
}

template <::std::integral char_type>
inline constexpr void scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, get_u32>, char_type const* p, get_u32 out) noexcept
{
    *out.out = load_le(p, 4u);
}

template <::std::integral char_type>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, get_u64>) noexcept
{
    return 8u;
}

template <::std::integral char_type>
inline constexpr void scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, get_u64>, char_type const* p, get_u64 out) noexcept
{
    *out.out = load_le(p, 8u);
}

template <::std::integral char_type>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, get_u16_may_error>) noexcept
{
    return 2u;
}

template <::std::integral char_type>
inline constexpr ::fast_io::parse_code scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, get_u16_may_error>,
                                                                   char_type const* p,
                                                                   get_u16_may_error out) noexcept
{
    *out.out = load_le(p, 2u);
    return ::fast_io::parse_code::ok;
}

template <::std::integral char_type>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, get_u16_end_of_file>) noexcept
{
    return 2u;
}

template <::std::integral char_type>
inline constexpr ::fast_io::parse_code scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, get_u16_end_of_file>,
                                                                   char_type const* p,
                                                                   get_u16_end_of_file out) noexcept
{
    *out.out = load_le(p, 2u);
    return ::fast_io::parse_code::end_of_file;
}

template <::std::integral char_type, ::std::size_t n>
inline constexpr ::std::size_t scan_precise_reserve_size(::fast_io::io_reserve_type_t<char_type, unit_get<char_type, n>>) noexcept
{
    return n;
}

template <::std::integral char_type, ::std::size_t n>
inline constexpr void scan_precise_reserve_define(::fast_io::io_reserve_type_t<char_type, unit_get<char_type, n>>,
                                                  char_type const* p,
                                                  unit_get<char_type, n> out) noexcept
{
    ::std::uint64_t value{};
    for(::std::size_t i{}; i != n; ++i)
    {
        value += static_cast<::std::uint64_t>(p[i]) << (i * 8u);
    }
    *out.out = value;
}

template <::std::integral char_type>
inline constexpr ::fast_io::io_type_t<context3_state> scan_context_type(::fast_io::io_reserve_type_t<char_type, context3>) noexcept
{
    return {};
}

template <::std::integral char_type>
inline constexpr ::fast_io::parse_result<char_type const*> scan_context_define(::fast_io::io_reserve_type_t<char_type, context3>,
                                                                              context3_state&,
                                                                              char_type const* first,
                                                                              char_type const* last,
                                                                              context3 out) noexcept
{
    if(static_cast<::std::size_t>(last - first) < 3u)
    {
        return {last, ::fast_io::parse_code::partial};
    }
    *out.out = load_le(first, 3u);
    return {first + 3u, ::fast_io::parse_code::ok};
}

template <::std::integral char_type>
inline constexpr ::fast_io::parse_code scan_context_eof_define(::fast_io::io_reserve_type_t<char_type, context3>, context3_state&, context3) noexcept
{
    return ::fast_io::parse_code::end_of_file;
}

    static_assert(::fast_io::precise_reserve_scannable_no_error<char, get_u32>);
    static_assert(!::fast_io::precise_reserve_scannable_no_error<char, get_u16_may_error>);
    static_assert(::fast_io::details::decay::find_continuous_precise_scan_n<char, get_u8, get_u16, get_u32, get_u64>().position == 4u);
    static_assert(::fast_io::details::decay::find_continuous_precise_scan_n<char, get_u8, get_u16, get_u32, get_u64>().neededspace == 15u);
    static_assert(::fast_io::details::decay::find_continuous_precise_scan_n<char, get_u32>().position == 1u);
    static_assert(::fast_io::details::decay::find_continuous_precise_scan_n<char, get_u16_may_error, get_u16_may_error>().position == 0u);
    static_assert(::fast_io::details::decay::find_continuous_precise_scan_n<char16_t, unit_get<char16_t, 2u>, unit_get<char16_t, 3u>>().neededspace == 5u);

    inline void fill_sequence(char* first, ::std::size_t n, unsigned char start) noexcept
    {
        for(::std::size_t i{}; i != n; ++i)
        {
            first[i] = static_cast<char>(static_cast<unsigned char>(start + static_cast<unsigned char>(i)));
        }
    }

    inline scan_input_state make_state(char* first, ::std::size_t n, char const* cold_first = nullptr, ::std::size_t cold_n = 0u) noexcept
    {
        scan_input_state state{first, first, first + n};
        state.cold_curr = cold_first;
        state.cold_end = cold_first == nullptr ? nullptr : cold_first + cold_n;
        return state;
    }

    int test_full_batch() noexcept
    {
        char buffer[32]{};
        fill_sequence(buffer, sizeof(buffer), 1u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};
        ::std::uint64_t c{};
        ::std::uint64_t d{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u32{&a},
                                                                                get_u32{&b},
                                                                                get_u64{&c},
                                                                                get_u8{&d}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 4u));
        UWVM_TEST_REQUIRE(b == load_le(buffer + 4u, 4u));
        UWVM_TEST_REQUIRE(c == load_le(buffer + 8u, 8u));
        UWVM_TEST_REQUIRE(d == load_le(buffer + 16u, 1u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 17u);
        UWVM_TEST_REQUIRE(state.curr_calls == 1u);
        UWVM_TEST_REQUIRE(state.end_calls == 1u);
        UWVM_TEST_REQUIRE(state.set_calls == 1u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 0u);
        return 0;
    }

    int test_mixed_run() noexcept
    {
        char buffer[40]{};
        fill_sequence(buffer, sizeof(buffer), 11u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};
        ::std::uint64_t middle{};
        ::std::uint64_t c{};
        ::std::uint64_t d{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u32{&a},
                                                                                get_u32{&b},
                                                                                context3{&middle},
                                                                                get_u64{&c},
                                                                                get_u64{&d}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 4u));
        UWVM_TEST_REQUIRE(b == load_le(buffer + 4u, 4u));
        UWVM_TEST_REQUIRE(middle == load_le(buffer + 8u, 3u));
        UWVM_TEST_REQUIRE(c == load_le(buffer + 11u, 8u));
        UWVM_TEST_REQUIRE(d == load_le(buffer + 19u, 8u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 27u);
        UWVM_TEST_REQUIRE(state.curr_calls == 3u);
        UWVM_TEST_REQUIRE(state.end_calls == 3u);
        UWVM_TEST_REQUIRE(state.set_calls == 3u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 0u);
        return 0;
    }

    int test_single_precise() noexcept
    {
        char buffer[8]{};
        fill_sequence(buffer, sizeof(buffer), 31u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state}, get_u32{&a}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 4u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 4u);
        UWVM_TEST_REQUIRE(state.curr_calls == 1u);
        UWVM_TEST_REQUIRE(state.end_calls == 1u);
        UWVM_TEST_REQUIRE(state.set_calls == 1u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 0u);
        return 0;
    }

    int test_may_error_precise_not_batched() noexcept
    {
        char buffer[8]{};
        fill_sequence(buffer, sizeof(buffer), 41u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u16_may_error{&a},
                                                                                get_u16_may_error{&b}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 2u));
        UWVM_TEST_REQUIRE(b == load_le(buffer + 2u, 2u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 4u);
        UWVM_TEST_REQUIRE(state.curr_calls == 2u);
        UWVM_TEST_REQUIRE(state.end_calls == 2u);
        UWVM_TEST_REQUIRE(state.set_calls == 2u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 0u);
        return 0;
    }

    int test_may_error_end_of_file_keeps_curr() noexcept
    {
        char buffer[8]{};
        fill_sequence(buffer, sizeof(buffer), 47u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{777u};

        UWVM_TEST_REQUIRE(!::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                 get_u16_end_of_file{&a},
                                                                                 get_u32{&b}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 2u));
        UWVM_TEST_REQUIRE(b == 777u);
        UWVM_TEST_REQUIRE(state.curr == buffer);
        UWVM_TEST_REQUIRE(state.curr_calls == 1u);
        UWVM_TEST_REQUIRE(state.end_calls == 1u);
        UWVM_TEST_REQUIRE(state.set_calls == 0u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 0u);
        return 0;
    }

    int test_error_batch_continuation() noexcept
    {
        char buffer[24]{};
        fill_sequence(buffer, sizeof(buffer), 91u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};
        ::std::uint64_t c{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u16_may_error{&a},
                                                                                get_u32{&b},
                                                                                get_u32{&c}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 2u));
        UWVM_TEST_REQUIRE(b == load_le(buffer + 2u, 4u));
        UWVM_TEST_REQUIRE(c == load_le(buffer + 6u, 4u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 10u);
        UWVM_TEST_REQUIRE(state.curr_calls == 2u);
        UWVM_TEST_REQUIRE(state.end_calls == 2u);
        UWVM_TEST_REQUIRE(state.set_calls == 2u);
        return 0;
    }

    int test_batch_error_continuation() noexcept
    {
        char buffer[24]{};
        fill_sequence(buffer, sizeof(buffer), 101u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};
        ::std::uint64_t c{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u32{&a},
                                                                                get_u32{&b},
                                                                                get_u16_may_error{&c}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 4u));
        UWVM_TEST_REQUIRE(b == load_le(buffer + 4u, 4u));
        UWVM_TEST_REQUIRE(c == load_le(buffer + 8u, 2u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 10u);
        UWVM_TEST_REQUIRE(state.curr_calls == 2u);
        UWVM_TEST_REQUIRE(state.end_calls == 2u);
        UWVM_TEST_REQUIRE(state.set_calls == 2u);
        return 0;
    }

    int test_batch_error_batch_error_batch_continuation() noexcept
    {
        char buffer[48]{};
        fill_sequence(buffer, sizeof(buffer), 111u);
        auto state{make_state(buffer, sizeof(buffer))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};
        ::std::uint64_t c{};
        ::std::uint64_t d{};
        ::std::uint64_t e{};
        ::std::uint64_t f{};
        ::std::uint64_t g{};
        ::std::uint64_t h{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u32{&a},
                                                                                get_u32{&b},
                                                                                get_u16_may_error{&c},
                                                                                get_u32{&d},
                                                                                get_u32{&e},
                                                                                get_u16_may_error{&f},
                                                                                get_u32{&g},
                                                                                get_u32{&h}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 4u));
        UWVM_TEST_REQUIRE(b == load_le(buffer + 4u, 4u));
        UWVM_TEST_REQUIRE(c == load_le(buffer + 8u, 2u));
        UWVM_TEST_REQUIRE(d == load_le(buffer + 10u, 4u));
        UWVM_TEST_REQUIRE(e == load_le(buffer + 14u, 4u));
        UWVM_TEST_REQUIRE(f == load_le(buffer + 18u, 2u));
        UWVM_TEST_REQUIRE(g == load_le(buffer + 20u, 4u));
        UWVM_TEST_REQUIRE(h == load_le(buffer + 24u, 4u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 28u);
        UWVM_TEST_REQUIRE(state.curr_calls == 5u);
        UWVM_TEST_REQUIRE(state.end_calls == 5u);
        UWVM_TEST_REQUIRE(state.set_calls == 5u);
        return 0;
    }

    int test_buffer_has_first_field_only() noexcept
    {
        char buffer[4]{};
        char cold[8]{};
        fill_sequence(buffer, sizeof(buffer), 51u);
        fill_sequence(cold, sizeof(cold), 61u);
        auto state{make_state(buffer, sizeof(buffer), cold, sizeof(cold))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};
        ::std::uint64_t c{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u32{&a},
                                                                                get_u32{&b},
                                                                                get_u32{&c}));
        UWVM_TEST_REQUIRE(a == load_le(buffer, 4u));
        UWVM_TEST_REQUIRE(b == load_le(cold, 4u));
        UWVM_TEST_REQUIRE(c == load_le(cold + 4u, 4u));
        UWVM_TEST_REQUIRE(state.curr == buffer + 4u);
        UWVM_TEST_REQUIRE(state.set_calls == 1u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 2u);
        return 0;
    }

    int test_buffer_shorter_than_first_field() noexcept
    {
        char buffer[2]{};
        char cold[8]{};
        fill_sequence(buffer, sizeof(buffer), 71u);
        fill_sequence(cold, sizeof(cold), 81u);
        auto state{make_state(buffer, sizeof(buffer), cold, sizeof(cold))};
        ::std::uint64_t a{};
        ::std::uint64_t b{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(scan_input_ref{&state},
                                                                                get_u32{&a},
                                                                                get_u32{&b}));
        UWVM_TEST_REQUIRE(a == load_le(cold, 4u));
        UWVM_TEST_REQUIRE(b == load_le(cold + 4u, 4u));
        UWVM_TEST_REQUIRE(state.curr == buffer);
        UWVM_TEST_REQUIRE(state.set_calls == 0u);
        UWVM_TEST_REQUIRE(state.read_all_calls == 2u);
        return 0;
    }

    template <::std::integral char_type>
    int test_char_type_units_one() noexcept
    {
        char_type buffer[16]{};
        for(::std::size_t i{}; i != sizeof(buffer) / sizeof(*buffer); ++i)
        {
            buffer[i] = static_cast<char_type>(i + 1u);
        }
        unit_input_state<char_type> state{buffer, buffer, buffer + 16u};
        ::std::uint64_t a{};
        ::std::uint64_t b{};

        UWVM_TEST_REQUIRE(::fast_io::operations::decay::scan_freestanding_decay(unit_input_ref<char_type>{&state},
                                                                                unit_get<char_type, 2u>{&a},
                                                                                unit_get<char_type, 3u>{&b}));
        UWVM_TEST_REQUIRE(a != 0u);
        UWVM_TEST_REQUIRE(b != 0u);
        UWVM_TEST_REQUIRE(state.curr == buffer + 5u);
        UWVM_TEST_REQUIRE(state.curr_calls == 1u);
        UWVM_TEST_REQUIRE(state.end_calls == 1u);
        UWVM_TEST_REQUIRE(state.set_calls == 1u);
        return 0;
    }

    int test_char_type_units() noexcept
    {
        if(auto const ret{test_char_type_units_one<char>()})
        {
            return ret;
        }
        if(auto const ret{test_char_type_units_one<char8_t>()})
        {
            return ret;
        }
        if(auto const ret{test_char_type_units_one<char16_t>()})
        {
            return ret;
        }
        if(auto const ret{test_char_type_units_one<char32_t>()})
        {
            return ret;
        }
        if(auto const ret{test_char_type_units_one<wchar_t>()})
        {
            return ret;
        }
        return 0;
    }
}  // namespace uwvm2_test_scan_precise_batch

int main()
{
    using namespace uwvm2_test_scan_precise_batch;

    if(auto const ret{test_full_batch()})
    {
        return ret;
    }
    if(auto const ret{test_mixed_run()})
    {
        return ret;
    }
    if(auto const ret{test_single_precise()})
    {
        return ret;
    }
    if(auto const ret{test_may_error_precise_not_batched()})
    {
        return ret;
    }
    if(auto const ret{test_may_error_end_of_file_keeps_curr()})
    {
        return ret;
    }
    if(auto const ret{test_error_batch_continuation()})
    {
        return ret;
    }
    if(auto const ret{test_batch_error_continuation()})
    {
        return ret;
    }
    if(auto const ret{test_batch_error_batch_error_batch_continuation()})
    {
        return ret;
    }
    if(auto const ret{test_buffer_has_first_field_only()})
    {
        return ret;
    }
    if(auto const ret{test_buffer_shorter_than_first_field()})
    {
        return ret;
    }
    if(auto const ret{test_char_type_units()})
    {
        return ret;
    }
}

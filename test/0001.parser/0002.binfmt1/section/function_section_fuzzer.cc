/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-06-15
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
#include <type_traits>
#include <concepts>
#include <memory>
#include <random>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

#if !defined(__cpp_exceptions)
# error "please enable cpp_exception first to run this fuzzer"
#endif

#if !((defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK))
# error "please enable a full check"
#endif

namespace test
{
    inline constexpr void print_section(::std::byte const* const section_begin, ::std::byte const* const section_end) noexcept
    {
        ::fast_io::io::perr("Input: [");
        for(auto section_curr{section_begin}; section_curr != section_end; ++section_curr)
        {
            ::fast_io::io::perr(fast_io::mnp::hexupper<false, true>(::std::to_integer<::std::uint_least8_t>(*section_curr)), " ");
        }
        ::fast_io::io::perr("]\n");
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void check_function_section([[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                                                     ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t> sec_adl,
                                                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                                 ::std::byte const* const section_begin,
                                                 ::std::byte const* const section_end,
                                                 ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count) noexcept
    {
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get function_section_storage_t from storages
        auto const& functionsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(
                module_storage.sections)};

        // check has typesec
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};

        auto section_curr{section_begin};

        // [before_section ... ] | func_count typeidx1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};  // use for check

        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | func_count ...] typeidx1 ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        // handle it
        auto const type_section_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(typesec.types.size())};

        ::std::size_t index{};

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check function counter
            // Ensure content is available before counting (section_curr != section_end)

            auto const func_counter_end{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_counter + 1u)};

            // check overflow
            if (func_counter_end < func_counter) [[unlikely]]
            {
                // It never happens in the use cases tested
                print_section(section_begin, section_end);
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            func_counter = func_counter_end;

            if(func_counter > func_count) [[unlikely]]
            {
                print_section(section_begin, section_end);
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            // [ ... typeidx1] ... typeidx2 ...
            // [     safe    ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 typeidx;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(typeidx))};

            if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                print_section(section_begin, section_end);
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check: type_index should less than type_section_count
            if(typeidx >= type_section_count) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }

            if(typeidx != functionsec.funcs.index_unchecked(index)) [[unlikely]]
            {
                print_section(section_begin, section_end);
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

            // [ ... typeidx1 ...] typeidx2 ...
            // [      safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr

            ++index;
        }

        // [before_section ... | func_count ... typeidx1 ... ...] (end)
        // [                       safe                         ] unsafe (could be the section_end)
        //                                                        ^^ section_curr
    }

    struct memory_safety_checker_allocator
    {
        char* ptr{};
        ::std::size_t n{};

        inline constexpr memory_safety_checker_allocator(char const* begin, char const* end) noexcept
        {
            if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }
            else if(begin == end) [[unlikely]] { return; }

            n = static_cast<::std::size_t>(end - begin);
            ptr = reinterpret_cast<char*>(::std::malloc(n));

            if(ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::fast_io::freestanding::my_memcpy(ptr, begin, n * sizeof(char));
        }

        inline constexpr memory_safety_checker_allocator(memory_safety_checker_allocator const&) noexcept = delete;
        inline constexpr memory_safety_checker_allocator(memory_safety_checker_allocator&&) noexcept = delete;
        inline constexpr memory_safety_checker_allocator& operator= (memory_safety_checker_allocator const&) noexcept = delete;
        inline constexpr memory_safety_checker_allocator& operator= (memory_safety_checker_allocator&&) noexcept = delete;

        inline constexpr ~memory_safety_checker_allocator() noexcept
        {
            // Multiple calls to the destructor are ub
            ::std::free(ptr);
        }

        inline constexpr char const* cbegin() const noexcept { return ptr; }

        inline constexpr char const* cend() const noexcept { return ptr + n; }
    };
}  // namespace test

int main()
{
    [[maybe_unused]] ::fast_io::ibuf_white_hole_engine eng;
    [[maybe_unused]] ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> func_count_eng{8u, 1024u};

    [[maybe_unused]] constexpr auto u8_1b_errval{
        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u)};
    [[maybe_unused]] constexpr auto u8_2b_errval{
        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u)};
    [[maybe_unused]] constexpr auto u16_2b_errval{
        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u)};
    [[maybe_unused]] constexpr auto u16_3b_errval{
        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 16u)};
    // u32_5b no error value

    [[maybe_unused]] ::std::uniform_int_distribution<unsigned> error_rate_eng{0u, 99u};

    // u163b and u325b use conventional algorithms that do not need to be checked

    // all correct u81b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u81b_in_curr_round{
            0u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u) - 2u};

        auto const max_u81b_in_curr_round{generate_max_u81b_in_curr_round(eng)};
        auto const typeidxmax{max_u81b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_1b{0u, max_u81b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u8_1b(eng)};
            ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_1b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // all correct u82b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u82b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u) - 2u};

        auto const max_u82b_in_curr_round{generate_max_u82b_in_curr_round(eng)};
        auto const typeidxmax{max_u82b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_2b{0u, max_u82b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u8_2b(eng)};
            ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // all correct u162b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u162b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u) - 2u};
        auto const max_u162b_in_curr_round{generate_max_u162b_in_curr_round(eng)};
        auto const typeidxmax{max_u162b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u16_2b{0u, max_u162b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u16_2b(eng)};
            ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u16_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // all correct with redundancies u81b
    for(unsigned i{}; i != 10'000u; ++i)
    {

        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u81b_in_curr_round{
            0u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u) - 2u};

        auto const max_u81b_in_curr_round{generate_max_u81b_in_curr_round(eng)};
        auto const typeidxmax{max_u81b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_1b{0u, max_u81b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u8_1b(eng)};

            if(error_rate_eng(eng) < error_rate) { ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr)); }
            else
            {
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_1b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // all correct with redundancies u82b
    for(unsigned i{}; i != 10'000u; ++i)
    {

        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u82b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u) - 2u};

        auto const max_u82b_in_curr_round{generate_max_u82b_in_curr_round(eng)};
        auto const typeidxmax{max_u82b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_2b{0u, max_u82b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u8_2b(eng)};

            if(error_rate_eng(eng) < error_rate) { ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr)); }
            else
            {
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // all correct with redundancies u162b
    for(unsigned i{}; i != 10'000u; ++i)
    {

        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u162b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u) - 2u};

        auto const max_u162b_in_curr_round{generate_max_u162b_in_curr_round(eng)};
        auto const typeidxmax{max_u162b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u16_2b{0u, max_u162b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u16_2b(eng)};

            if(error_rate_eng(eng) < error_rate) { ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr)); }
            else
            {
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u16_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too large u81b
    for(unsigned i{}; i != 10'000u; ++i)
    {

        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u81b_in_curr_round{
            0u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u) - 2u};

        auto const max_u81b_in_curr_round{generate_max_u81b_in_curr_round(eng)};
        auto const typeidxmax{max_u81b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_1b{0u, max_u81b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u8_1b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u8_1b_errval));
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_1b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too large u82b
    for(unsigned i{}; i != 10'000u; ++i)
    {

        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u82b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u) - 2u};

        auto const max_u82b_in_curr_round{generate_max_u82b_in_curr_round(eng)};
        auto const typeidxmax{max_u82b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_2b{0u, max_u82b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u8_2b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u8_2b_errval));
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too large u162b
    for(unsigned i{}; i != 10'000u; ++i)
    {

        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u162b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u) - 2u};

        auto const max_u162b_in_curr_round{generate_max_u162b_in_curr_round(eng)};
        auto const typeidxmax{max_u162b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u16_2b{0u, max_u162b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u16_2b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u16_2b_errval));
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u16_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too large and error u81b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u81b_in_curr_round{
            0u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u) - 2u};

        auto const max_u81b_in_curr_round{generate_max_u81b_in_curr_round(eng)};
        auto const typeidxmax{max_u81b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_1b{0u, max_u81b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u8_1b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u8_1b_errval));
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_1b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too large and error u82b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u82b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u) - 2u};

        auto const max_u82b_in_curr_round{generate_max_u82b_in_curr_round(eng)};
        auto const typeidxmax{max_u82b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_2b{0u, max_u82b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u8_2b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u8_2b_errval));
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too large and error u162b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u162b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u) - 2u};

        auto const max_u162b_in_curr_round{generate_max_u162b_in_curr_round(eng)};
        auto const typeidxmax{max_u162b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u16_2b{0u, max_u162b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u16_2b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u16_2b_errval));
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u16_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too long u81b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u81b_in_curr_round{
            0u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u) - 2u};

        auto const max_u81b_in_curr_round{generate_max_u81b_in_curr_round(eng)};
        auto const typeidxmax{max_u81b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_1b{0u, max_u81b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u8_1b(eng)};
            ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));

            if(!(error_rate_eng(eng) < error_rate))
            {
                has_catch = false;  // Prevents no illegal values from being generated

                for(unsigned k{}; k != 6u; ++k)
                {
                    // The former has been exported and is memory safe.
                    *(buf_ov.curr_ptr - 1u) |= 0x80;
                    ::fast_io::io::print(buf_ov, "\0");
                }
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_1b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too long u82b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u82b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u) - 2u};

        auto const max_u82b_in_curr_round{generate_max_u82b_in_curr_round(eng)};
        auto const typeidxmax{max_u82b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_2b{0u, max_u82b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u8_2b(eng)};
            ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));

            if(!(error_rate_eng(eng) < error_rate))
            {
                has_catch = false;  // Prevents no illegal values from being generated

                for(unsigned k{}; k != 6u; ++k)
                {
                    // The former has been exported and is memory safe.
                    *(buf_ov.curr_ptr - 1u) |= 0x80;
                    ::fast_io::io::print(buf_ov, "\0");
                }
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // too long u162b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u162b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u) - 2u};

        auto const max_u162b_in_curr_round{generate_max_u162b_in_curr_round(eng)};
        auto const typeidxmax{max_u162b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u16_2b{0u, max_u162b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            auto const curr{curr_range_u16_2b(eng)};
            ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));

            if(!(error_rate_eng(eng) < error_rate))
            {
                has_catch = false;  // Prevents no illegal values from being generated

                for(unsigned k{}; k != 6u; ++k)
                {
                    // The former has been exported and is memory safe.
                    *(buf_ov.curr_ptr - 1u) |= 0x80;
                    ::fast_io::io::print(buf_ov, "\0");
                }
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u16_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // hybird u81b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u81b_in_curr_round{
            0u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u) - 2u};

        auto const max_u81b_in_curr_round{generate_max_u81b_in_curr_round(eng)};
        auto const typeidxmax{max_u81b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_1b{0u, max_u81b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u8_1b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u8_1b_errval));
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                has_catch = false;  // Prevents no illegal values from being generated

                for(unsigned k{}; k != 6u; ++k)
                {
                    // The former has been exported and is memory safe.
                    *(buf_ov.curr_ptr - 1u) |= 0x80;
                    ::fast_io::io::print(buf_ov, "\0");
                }
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_1b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // hybird u82b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u82b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 7u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u) - 2u};

        auto const max_u82b_in_curr_round{generate_max_u82b_in_curr_round(eng)};
        auto const typeidxmax{max_u82b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u8_2b{0u, max_u82b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u8_2b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u8_2b_errval));
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                has_catch = false;  // Prevents no illegal values from being generated

                for(unsigned k{}; k != 6u; ++k)
                {
                    // The former has been exported and is memory safe.
                    *(buf_ov.curr_ptr - 1u) |= 0x80;
                    ::fast_io::io::print(buf_ov, "\0");
                }
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u8_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }

    // hybird u162b
    for(unsigned i{}; i != 10'000u; ++i)
    {
        auto const error_rate{i % 100u};

        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> generate_max_u162b_in_curr_round{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 8u,
            (static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u) << 14u) - 2u};

        auto const max_u162b_in_curr_round{generate_max_u162b_in_curr_round(eng)};
        auto const typeidxmax{max_u162b_in_curr_round + 1u};
        ::std::uniform_int_distribution<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> curr_range_u16_2b{0u, max_u162b_in_curr_round};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_count{func_count_eng(eng)};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_counter{};

        char buf[16384];  // 8 * 1024 max
        ::fast_io::obuffer_view buf_ov{buf, buf + sizeof(buf)};

        bool has_catch{true};

        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 j{}; j != func_count; ++j)
        {
            if(error_rate_eng(eng) < error_rate)
            {
                auto const curr{curr_range_u16_2b(eng)};
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(curr));
            }
            else
            {
                has_catch = false;  // Prevents no illegal values from being generated
                ::fast_io::io::print(buf_ov, ::fast_io::mnp::leb128_put(u16_2b_errval));
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                // The former has been exported and is memory safe.
                *(buf_ov.curr_ptr - 1u) |= 0x80;
                ::fast_io::io::print(buf_ov, "\0");
            }

            if(!(error_rate_eng(eng) < error_rate))
            {
                has_catch = false;  // Prevents no illegal values from being generated

                for(unsigned k{}; k != 6u; ++k)
                {
                    // The former has been exported and is memory safe.
                    *(buf_ov.curr_ptr - 1u) |= 0x80;
                    ::fast_io::io::print(buf_ov, "\0");
                }
            }
        }

        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1> strg{};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>>(strg.sections)};
        typesec.types.resize(static_cast<::std::size_t>(typeidxmax));

        ::uwvm2::parser::wasm::base::error_impl e{};

        // Verifying Memory Safety (need asan)
        ::test::memory_safety_checker_allocator memory_safety_checker{buf_ov.cbegin(), buf_ov.cend()};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::scan_function_section_impl_u16_2b<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                e,
                {},
                func_counter,
                func_count);

            if(func_counter != func_count) [[unlikely]]
            {
                ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                      reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            ::test::check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                strg,
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()),
                func_count);
        }
        catch(::fast_io::error)
        {
            has_catch = true;
        }

        if(!has_catch)
        {
            ::test::print_section(reinterpret_cast<::std::byte const*>(memory_safety_checker.cbegin()),
                                  reinterpret_cast<::std::byte const*>(memory_safety_checker.cend()));
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
    }
}

// macro
#include <uwvm2/utils/macro/pop_macros.h>

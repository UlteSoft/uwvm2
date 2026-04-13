/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-12
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
#include <type_traits>
#include <random>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/vector.h>
# include <fast_io_dsal/string.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/utf/impl.h>
#else
# error "Module testing is not currently supported"
#endif

inline ::uwvm2::utils::utf::u8result reference_check_legal_utf8_rfc3629_unchecked(char8_t const* str_begin, char8_t const* str_end) noexcept
{
    char8_t const* current = str_begin;
    while(current < str_end)
    {
        ::std::uint_least8_t byte = (*current & 0xFFu);
        ::std::size_t len = 0;

        // 1-byte character
        if(byte < 0x80)
        {
            ++current;
            continue;
        }
        // 2-byte character
        else if((byte & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u)) { len = 2; }
        // 3-byte character
        else if((byte & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u)) { len = 3; }
        // 4-byte character
        else if((byte & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u)) { len = 4; }
        // Illegal start byte
        else
        {
            if((byte & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
            {
                return {current, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
            }
            else
            {
                return {current, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
            }
        }

        // Check if the sequence length is sufficient
        if(current + len > str_end) { return {current, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

        // Check subsequent byte format (10xxxxxx)
        for(size_t i = 1; i < len; ++i)
        {
            if((current[i] & 0xC0) != 0x80) { return {current, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }
        }

        // Calculate code points and verify
        char32_t cp = 0;
        if(len == 2)
        {
            cp = ((static_cast<::std::uint_least32_t>(byte) & 0x1F) << 6) | (current[1] & 0x3F);
            // Checking for excessively long codes
            if(cp < 0x80) { return {current, ::uwvm2::utils::utf::utf_error_code::overlong_encoding}; }
        }
        else if(len == 3)
        {
            cp = ((static_cast<::std::uint_least32_t>(byte) & 0x0F) << 12) | ((static_cast<::std::uint_least32_t>(current[1]) & 0x3F) << 6) |
                 (current[2] & 0x3F);
            if(cp < 0x800) { return {current, ::uwvm2::utils::utf::utf_error_code::overlong_encoding}; }
            // Check the proxy area
            if(cp >= 0xD800 && cp <= 0xDFFF) { return {current, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate}; }
        }
        else if(len == 4)
        {
            cp = ((static_cast<::std::uint_least32_t>(byte) & 0x07) << 18) | ((static_cast<::std::uint_least32_t>(current[1]) & 0x3F) << 12) |
                 ((static_cast<::std::uint_least32_t>(current[2]) & 0x3F) << 6) | (current[3] & 0x3F);
            if(cp < 0x10000) { return {current, ::uwvm2::utils::utf::utf_error_code::overlong_encoding}; }
            // Check for exceeding U+10FFFF
            if(cp > 0x10FFFF) { return {current, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint}; }
        }

        current += len;
    }
    return {str_end, ::uwvm2::utils::utf::utf_error_code::success};
}

// Generate random test data
inline ::uwvm2::utils::container::u8string generate_random_utf8_data(::std::size_t min_len, ::std::size_t max_len, ::std::size_t probability) noexcept
{
    ::uwvm2::utils::container::u8string data;
    ::fast_io::ibuf_white_hole_engine eng;
    ::std::uniform_int_distribution<::std::size_t> len_engine{min_len, max_len};
    ::std::uniform_int_distribution<::std::size_t> probability_engine{0uz, 99uz};
    ::std::uniform_int_distribution<::std::size_t> byte_engine{0x00uz, 0xFFuz};
    ::std::size_t len = len_engine(eng);
    data.reserve(len);

    for(::std::size_t i = 0; i < len; ++i)
    {
        // Controlling the probability of generating a valid UTF-8
        if(probability_engine(eng) < probability)
        {  // 80% probability of generating valid bytes
            ::std::size_t byte_type = byte_engine(eng) % 4uz;
            switch(byte_type)
            {
                case 0: data.push_back(byte_engine(eng) % 0x80); break;
                case 1: data.push_back(0x80 + (byte_engine(eng) % 0x40)); break;
                case 2: data.push_back(0xC0 + (byte_engine(eng) % 0x20)); break;
                case 3: data.push_back(0xE0 + (byte_engine(eng) % 0x10)); break;
            }
        }
        else
        {
            data.push_back(byte_engine(eng));
        }
    }
    return data;
}

// Fuzzer Test Master Function
inline void run_fuzzer_tests(::std::size_t num_tests) noexcept
{
    for(::std::size_t i = 0; i < num_tests; ++i)
    {
        auto data_vec = generate_random_utf8_data(8, 1024, i % 100uz);
        if(data_vec.empty()) { continue; }

        char8_t const* data = reinterpret_cast<char8_t const*>(data_vec.data());
        char8_t const* end = data + data_vec.size();

        // reference implementation
        ::uwvm2::utils::utf::u8result ref_result = reference_check_legal_utf8_rfc3629_unchecked(data, end);

        // Test implementation (zero_illegal = false)
        ::uwvm2::utils::utf::u8result test_result_false = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>(data, end);

        // Comparative results
        if(test_result_false.err != ref_result.err || test_result_false.pos != ref_result.pos)
        {
            ::fast_io::io::perr("Test case #", i, " failed (zero_illegal=false):\n");
            ::fast_io::io::perr("  Input: ");
            for(auto b: data_vec) { ::fast_io::io::perr(fast_io::mnp::hexupper<false, true>(b), " "); }
            ::fast_io::io::perrln("\n  Expected: err=",
                                  static_cast<int>(ref_result.err),
                                  ", pos=",
                                  (ref_result.pos - data),
                                  "\n  Actual: err=",
                                  static_cast<int>(test_result_false.err),
                                  ", pos=",
                                  (test_result_false.pos - data));
            ::fast_io::fast_terminate();
        }

        // Test implementation (zero_illegal = true)
        ::uwvm2::utils::utf::u8result test_result_true = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<true>(data, end);

        char8_t const* first_null = nullptr;
        for(char8_t const* p = data; p != end; ++p)
        {
            if(*p == 0)
            {
                first_null = p;
                break;
            }
        }

        if(first_null && test_result_true.err == ::uwvm2::utils::utf::utf_error_code::contains_empty_characters)
        {
            if(test_result_true.pos != first_null)
            {
                ::fast_io::io::perr("Test case #", i, " failed (zero_illegal=true):\n");
                ::fast_io::io::perr("  Input: ");
                for(auto b: data_vec) { ::fast_io::io::perr(::fast_io::mnp::hexupper<false, true>(b), " "); }
                ::fast_io::io::perrln("  Expected: contains_empty_characters at ", (first_null - data));
                ::fast_io::io::perrln("  Actual: err=", static_cast<int>(test_result_true.err), ", pos=", (test_result_true.pos - data));
                ::fast_io::fast_terminate();
            }
        }
    }
}

// Fixed-length random read tests for lengths: 0..512
inline void run_fixed_length_random_read_tests(::std::size_t runs_per_length) noexcept
{
    for(::std::size_t len = 0uz; len <= 512uz; ++len)
    {
        for(::std::size_t i = 0; i < runs_per_length; ++i)
        {
            auto data_vec = generate_random_utf8_data(len, len, i % 100uz);
            if(data_vec.empty()) { continue; }

            char8_t const* data = reinterpret_cast<char8_t const*>(data_vec.data());
            char8_t const* end = data + data_vec.size();

            // reference implementation
            ::uwvm2::utils::utf::u8result ref_result = reference_check_legal_utf8_rfc3629_unchecked(data, end);

            // Test implementation (zero_illegal = false)
            ::uwvm2::utils::utf::u8result test_result_false = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>(data, end);

            // Comparative results
            if(test_result_false.err != ref_result.err || test_result_false.pos != ref_result.pos)
            {
                ::fast_io::io::perr("Fixed-length test failed (zero_illegal=false):\n");
                ::fast_io::io::perr("  Length: ", len, ", Run: ", i, "\n");
                ::fast_io::io::perr("  Input: ");
                for(auto b: data_vec) { ::fast_io::io::perr(::fast_io::mnp::hexupper<false, true>(b), " "); }
                ::fast_io::io::perrln("\n  Expected: err=",
                                      static_cast<int>(ref_result.err),
                                      ", pos=",
                                      (ref_result.pos - data),
                                      "\n  Actual: err=",
                                      static_cast<int>(test_result_false.err),
                                      ", pos=",
                                      (test_result_false.pos - data));
                ::fast_io::fast_terminate();
            }

            // Test implementation (zero_illegal = true)
            ::uwvm2::utils::utf::u8result test_result_true = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<true>(data, end);

            char8_t const* first_null = nullptr;
            for(char8_t const* p = data; p != end; ++p)
            {
                if(*p == 0)
                {
                    first_null = p;
                    break;
                }
            }

            if(first_null && test_result_true.err == ::uwvm2::utils::utf::utf_error_code::contains_empty_characters)
            {
                if(test_result_true.pos != first_null)
                {
                    ::fast_io::io::perr("Fixed-length test failed (zero_illegal=true):\n");
                    ::fast_io::io::perr("  Length: ", len, ", Run: ", i, "\n");
                    ::fast_io::io::perr("  Input: ");
                    for(auto b: data_vec) { ::fast_io::io::perr(::fast_io::mnp::hexupper<false, true>(b), " "); }
                    ::fast_io::io::perrln("  Expected: contains_empty_characters at ", (first_null - data));
                    ::fast_io::io::perrln("  Actual: err=", static_cast<int>(test_result_true.err), ", pos=", (test_result_true.pos - data));
                    ::fast_io::fast_terminate();
                }
            }
        }
    }
}

int main()
{
    constexpr size_t NUM_TESTS = 100'000;
    ::fast_io::io::perr("Running ", NUM_TESTS, " random UTF-8 fuzzer tests...\n");
    run_fuzzer_tests(NUM_TESTS);

    constexpr size_t RUNS_PER_LENGTH = 1'000;
    ::fast_io::io::perr("Running fixed-length random read tests (0..512) x ", RUNS_PER_LENGTH, "...\n");
    run_fixed_length_random_read_tests(RUNS_PER_LENGTH);

    ::fast_io::io::perr("All tests passed successfully!\n");
    return 0;
}


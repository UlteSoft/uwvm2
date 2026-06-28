/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-28
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>

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

#if defined(_WIN32) && !defined(__WINE__) && !defined(__CYGWIN__) && !defined(__BIONIC__) && !defined(_WIN32_WINDOWS)

namespace uwvm2_test_windows_file_loader_padding
{
    inline constexpr ::std::uint_least32_t file_share_all{0x00000001u | 0x00000002u | 0x00000004u};
    inline constexpr ::std::uint_least32_t file_attribute_readonly{0x00000001u};
    inline constexpr ::std::uint_least32_t file_attribute_normal{0x00000080u};
    inline constexpr ::std::uint_least32_t generic_read{0x80000000u};
    inline constexpr ::std::uint_least32_t generic_write{0x40000000u};
    inline constexpr ::std::uint_least32_t mem_mapped{0x00040000u};
    inline constexpr ::std::uint_least32_t mem_private{0x00020000u};

    enum class mapped_write_semantics : unsigned
    {
        read_only,
        private_copy,
        shared_writeback
    };

    inline ::std::size_t current_payload_size{};
    inline ::std::size_t current_extra_size{};
    inline mapped_write_semantics current_semantics{};
    inline bool current_readonly_file{};

    inline bool is_invalid_handle(void const* handle) noexcept
    {
        return handle == reinterpret_cast<void const*>(static_cast<::std::ptrdiff_t>(-1));
    }

    struct byte_buffer
    {
        ::std::byte* data{};
        ::std::size_t size{};

        inline explicit byte_buffer(::std::size_t n) : data{n == 0 ? nullptr : new ::std::byte[n]}, size{n} {}
        byte_buffer(byte_buffer const&) = delete;
        byte_buffer& operator=(byte_buffer const&) = delete;

        inline ~byte_buffer()
        {
            delete[] data;
        }
    };

    struct handle_guard
    {
        void* handle{};

        inline explicit constexpr handle_guard(void* hd = nullptr) noexcept : handle{hd} {}
        handle_guard(handle_guard const&) = delete;
        handle_guard& operator=(handle_guard const&) = delete;

        inline ~handle_guard()
        {
            if(handle != nullptr && !is_invalid_handle(handle))
            {
                ::fast_io::win32::CloseHandle(handle);
            }
        }
    };

    struct file_guard
    {
        char16_t const* path{};

        inline explicit constexpr file_guard(char16_t const* p) noexcept : path{p} {}
        file_guard(file_guard const&) = delete;
        file_guard& operator=(file_guard const&) = delete;

        inline ~file_guard()
        {
            if(path != nullptr)
            {
                ::fast_io::win32::SetFileAttributesW(path, file_attribute_normal);
                ::fast_io::win32::DeleteFileW(path);
            }
        }
    };

    inline char16_t* append_literal(char16_t* out, char16_t const* literal) noexcept
    {
        while(*literal != 0)
        {
            *out++ = *literal++;
        }
        return out;
    }

    inline char16_t* append_hex(char16_t* out, ::std::uint_least64_t value) noexcept
    {
        constexpr char16_t digits[]{u"0123456789abcdef"};
        bool emitted{};
        for(int shift{60}; shift >= 0; shift -= 4)
        {
            auto const nibble{static_cast<unsigned>((value >> static_cast<unsigned>(shift)) & 0x0fu)};
            if(nibble != 0u || emitted || shift == 0)
            {
                emitted = true;
                *out++ = digits[nibble];
            }
        }
        return out;
    }

    inline ::std::byte payload_byte(::std::size_t index) noexcept
    {
        return static_cast<::std::byte>((index * 131uz + 17uz) & 0xffuz);
    }

    inline ::std::byte expected_payload_byte(::std::size_t index, ::std::size_t payload_size,
                                             bool shared_writeback) noexcept
    {
        if(shared_writeback && payload_size != 0)
        {
            if(index + 1uz == payload_size)
            {
                return static_cast<::std::byte>(0x6b);
            }
            if(index == 0)
            {
                return static_cast<::std::byte>(0x5a);
            }
        }
        return payload_byte(index);
    }

    inline ::std::size_t round_up_to_page(::std::size_t value, ::std::size_t page_size)
    {
        if(value == 0)
        {
            return 0;
        }
        auto const rem{value % page_size};
        return rem == 0 ? value : value + (page_size - rem);
    }

    inline ::std::size_t get_page_size()
    {
        ::fast_io::win32::nt::system_basic_information sb{};
        auto const status{::fast_io::win32::nt::nt_query_system_information<false>(
            ::fast_io::win32::nt::system_information_class::SystemBasicInformation,
            __builtin_addressof(sb), static_cast<::std::uint_least32_t>(sizeof(sb)), nullptr)};
        UWVM_TEST_REQUIRE(status == 0);
        UWVM_TEST_REQUIRE(sb.PageSize != 0);
        return sb.PageSize;
    }

    template <::std::size_t n>
    inline int make_temp_path(char16_t (&path)[n])
    {
        static_assert(n >= 512uz);
        auto const temp_len{::fast_io::win32::GetTempPathW(static_cast<::std::uint_least32_t>(n), path)};
        UWVM_TEST_REQUIRE(temp_len != 0u);
        UWVM_TEST_REQUIRE(temp_len < 400u);
        char16_t* it{path + temp_len};
        it = append_literal(it, u"uwvm2_file_loader_padding_");
        it = append_hex(it, ::fast_io::win32::GetCurrentProcessId());
        *it++ = u'_';
        ::std::int_least64_t counter{};
        ::fast_io::win32::QueryPerformanceCounter(__builtin_addressof(counter));
        it = append_hex(it, static_cast<::std::uint_least64_t>(counter));
        it = append_literal(it, u".bin");
        *it = 0;
        return 0;
    }

    inline int write_payload(char16_t const* path, ::std::size_t payload_size)
    {
        void* const raw_handle{::fast_io::win32::CreateFileW(
            path, generic_write, file_share_all, nullptr, 2u /*CREATE_ALWAYS*/, file_attribute_normal, nullptr)};
        UWVM_TEST_REQUIRE(!is_invalid_handle(raw_handle));
        handle_guard guard{raw_handle};
        byte_buffer payload{payload_size};
        for(::std::size_t i{}; i != payload_size; ++i)
        {
            payload.data[i] = payload_byte(i);
        }
        if(payload_size != 0)
        {
            ::std::uint_least32_t written{};
            UWVM_TEST_REQUIRE(::fast_io::win32::WriteFile(raw_handle, payload.data,
                                                          static_cast<::std::uint_least32_t>(payload_size),
                                                          __builtin_addressof(written), nullptr));
            UWVM_TEST_REQUIRE(written == static_cast<::std::uint_least32_t>(payload_size));
        }
        return 0;
    }

    inline int file_size_is(char16_t const* path, ::std::size_t expected_size)
    {
        ::fast_io::win32_file file{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        UWVM_TEST_REQUIRE(::fast_io::file_size(file) == expected_size);
        return 0;
    }

    inline int file_content_is_payload(char16_t const* path, ::std::size_t payload_size, bool shared_writeback)
    {
        void* const raw_handle{::fast_io::win32::CreateFileW(
            path, generic_read, file_share_all, nullptr, 3u /*OPEN_EXISTING*/, file_attribute_normal, nullptr)};
        UWVM_TEST_REQUIRE(!is_invalid_handle(raw_handle));
        handle_guard guard{raw_handle};
        byte_buffer bytes{payload_size};
        if(payload_size != 0)
        {
            ::std::uint_least32_t bytes_read{};
            UWVM_TEST_REQUIRE(::fast_io::win32::ReadFile(raw_handle, bytes.data,
                                                         static_cast<::std::uint_least32_t>(payload_size),
                                                         __builtin_addressof(bytes_read), nullptr));
            UWVM_TEST_REQUIRE(bytes_read == static_cast<::std::uint_least32_t>(payload_size));
            for(::std::size_t i{}; i != payload_size; ++i)
            {
                UWVM_TEST_REQUIRE(bytes.data[i] == expected_payload_byte(i, payload_size, shared_writeback));
            }
        }
        return 0;
    }

    inline int virtual_query_type_is(void const* address, ::std::uint_least32_t expected_type)
    {
        ::fast_io::win32::memory_basic_information mbi{};
        UWVM_TEST_REQUIRE(::fast_io::win32::VirtualQuery(address, __builtin_addressof(mbi), sizeof(mbi)) != 0);
        UWVM_TEST_REQUIRE(mbi.Type == expected_type);
        return 0;
    }

    template <typename Loader, typename Options>
    inline int exercise_loader(char16_t const* path, Options const& options, ::fast_io::open_mode mode,
                               ::std::size_t payload_size, ::std::size_t page_size,
                               ::std::size_t extra_size, ::fast_io::file_loader_padding_mode padding_mode,
                               mapped_write_semantics semantics)
    {
        auto const writable{semantics != mapped_write_semantics::read_only};
        auto const shared_writeback{semantics == mapped_write_semantics::shared_writeback};
        current_payload_size = payload_size;
        current_extra_size = extra_size;
        current_semantics = semantics;
        current_readonly_file = (mode == ::fast_io::open_mode::in);
        {
            Loader loader{options, ::fast_io::mnp::os_c_str(path), mode};
            UWVM_TEST_REQUIRE(loader.size() == payload_size);
            UWVM_TEST_REQUIRE(loader.capacity() == payload_size + extra_size);
            UWVM_TEST_REQUIRE(loader.padding_size() == extra_size);
            auto* const data{loader.data()};
            if(payload_size + extra_size == 0)
            {
                UWVM_TEST_REQUIRE(data == nullptr);
            }
            else
            {
                UWVM_TEST_REQUIRE(data != nullptr);
            }

            for(::std::size_t i{}; i != payload_size; ++i)
            {
                UWVM_TEST_REQUIRE(static_cast<::std::byte>(data[i]) == payload_byte(i));
            }

            if(payload_size != 0)
            {
                UWVM_TEST_REQUIRE(virtual_query_type_is(data, mem_mapped) == 0);
            }
            else if(extra_size != 0)
            {
                UWVM_TEST_REQUIRE(virtual_query_type_is(data, mem_private) == 0);
            }
            if(extra_size != 0)
            {
                auto const file_region{round_up_to_page(payload_size, page_size)};
                if(payload_size != 0 && payload_size < file_region)
                {
                    UWVM_TEST_REQUIRE(virtual_query_type_is(data + payload_size, mem_mapped) == 0);
                    UWVM_TEST_REQUIRE(virtual_query_type_is(data + file_region - 1uz, mem_mapped) == 0);
                }
                if(payload_size + extra_size > file_region)
                {
                    UWVM_TEST_REQUIRE(virtual_query_type_is(data + file_region, mem_private) == 0);
                }
            }

            unsigned volatile padding_sum{};
            for(::std::size_t i{}; i != extra_size; ++i)
            {
                auto const value{data[payload_size + i]};
                padding_sum += static_cast<unsigned>(static_cast<unsigned char>(value));
                if(padding_mode == ::fast_io::file_loader_padding_mode::zero)
                {
                    UWVM_TEST_REQUIRE(value == 0);
                }
            }

            if(writable && payload_size != 0)
            {
                data[0] = static_cast<char>(0x5a);
                data[payload_size - 1uz] = static_cast<char>(0x6b);
            }
            if(writable && extra_size != 0)
            {
                data[payload_size] = static_cast<char>(0x55);
                data[payload_size + extra_size - 1uz] = static_cast<char>(0x66);
                auto const file_region{round_up_to_page(payload_size, page_size)};
                if(payload_size != 0 && payload_size < file_region && payload_size + extra_size > file_region)
                {
                    data[file_region - 1uz] = static_cast<char>(0x44);
                }
            }
            static_cast<void>(padding_sum);
            UWVM_TEST_REQUIRE(file_size_is(path, payload_size) == 0);
            UWVM_TEST_REQUIRE(file_content_is_payload(path, payload_size, shared_writeback) == 0);
        }
        UWVM_TEST_REQUIRE(file_size_is(path, payload_size) == 0);
        UWVM_TEST_REQUIRE(file_content_is_payload(path, payload_size, shared_writeback) == 0);
        return 0;
    }

    template <typename Loader, typename Options>
    inline int exercise_readonly_file(Options const& options, ::std::size_t payload_size, ::std::size_t page_size,
                                      ::std::size_t extra_size, ::fast_io::file_loader_padding_mode padding_mode,
                                      mapped_write_semantics semantics)
    {
        char16_t path[512]{};
        UWVM_TEST_REQUIRE(make_temp_path(path) == 0);
        file_guard cleanup{path};
        UWVM_TEST_REQUIRE(write_payload(path, payload_size) == 0);
        UWVM_TEST_REQUIRE(::fast_io::win32::SetFileAttributesW(path, file_attribute_readonly));
        UWVM_TEST_REQUIRE(exercise_loader<Loader>(path, options, ::fast_io::open_mode::in,
                                                  payload_size, page_size, extra_size, padding_mode,
                                                  semantics) == 0);
        return 0;
    }

    template <typename Loader, typename Options>
    inline int exercise_writable_handle(Options const& options, ::std::size_t payload_size,
                                        ::std::size_t page_size, ::std::size_t extra_size,
                                        ::fast_io::file_loader_padding_mode padding_mode,
                                        mapped_write_semantics semantics)
    {
        char16_t path[512]{};
        UWVM_TEST_REQUIRE(make_temp_path(path) == 0);
        file_guard cleanup{path};
        UWVM_TEST_REQUIRE(write_payload(path, payload_size) == 0);
        UWVM_TEST_REQUIRE(exercise_loader<Loader>(path, options,
                                                  ::fast_io::open_mode::in | ::fast_io::open_mode::out,
                                                  payload_size, page_size, extra_size, padding_mode,
                                                  semantics) == 0);
        return 0;
    }

    template <typename Loader, typename Options>
    inline int exercise_loader_matrix(Options const& options, ::std::size_t const* sizes,
                                      ::std::size_t sizes_count, ::std::size_t page_size,
                                      ::std::size_t extra_size,
                                      ::fast_io::file_loader_padding_mode padding_mode,
                                      mapped_write_semantics semantics, bool readonly_file)
    {
        for(::std::size_t i{}; i != sizes_count; ++i)
        {
            if(readonly_file)
            {
                UWVM_TEST_REQUIRE(exercise_readonly_file<Loader>(options, sizes[i], page_size, extra_size,
                                                                  padding_mode, semantics) == 0);
            }
            else
            {
                UWVM_TEST_REQUIRE(exercise_writable_handle<Loader>(options, sizes[i], page_size, extra_size,
                                                                    padding_mode, semantics) == 0);
            }
        }
        return 0;
    }

    inline int run()
    {
        auto const page_size{get_page_size()};
        ::std::size_t const sizes[]{
            0uz,
            1uz,
            4uz,
            page_size - 1uz,
            page_size,
            page_size + 17uz,
            page_size * 2uz + 3uz};

        ::std::size_t const extra_sizes[]{
            0uz,
            1uz,
            64uz,
            page_size + 17uz};

        constexpr ::fast_io::file_loader_padding_mode modes[]{
            ::fast_io::file_loader_padding_mode::zero,
            ::fast_io::file_loader_padding_mode::uninitialized};

        for(auto const padding_mode : modes)
        {
            for(auto const extra_size : extra_sizes)
            {
                auto const extra{::fast_io::file_loader_extra_bytes{extra_size, padding_mode}};
                auto const win32_private_options{::fast_io::win32_mmap_options{
                    ::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
                    ::fast_io::mmap_flags::map_private,
                    extra}};
                auto const nt_private_options{::fast_io::nt_mmap_options{
                    ::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
                    ::fast_io::mmap_flags::map_private,
                    extra}};
                auto const win32_shared_options{::fast_io::win32_mmap_options{
                    ::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
                    ::fast_io::mmap_flags::map_shared,
                    extra}};
                auto const nt_shared_options{::fast_io::nt_mmap_options{
                    ::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
                    ::fast_io::mmap_flags::map_shared,
                    extra}};
                auto const win32_shared_readonly_options{::fast_io::win32_mmap_options{
                    ::fast_io::mmap_prot::prot_read,
                    ::fast_io::mmap_flags::map_shared,
                    extra}};
                auto const nt_shared_readonly_options{::fast_io::nt_mmap_options{
                    ::fast_io::mmap_prot::prot_read,
                    ::fast_io::mmap_flags::map_shared,
                    extra}};

                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::win32_file_loader>(
                    win32_private_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::private_copy, true) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::win32_file_loader>(
                    win32_private_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::private_copy, false) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::win32_file_loader>(
                    win32_shared_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::shared_writeback, false) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::win32_file_loader>(
                    win32_shared_readonly_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::read_only, true) == 0);

                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::nt_file_loader>(
                    nt_private_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::private_copy, true) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::nt_file_loader>(
                    nt_private_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::private_copy, false) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::nt_file_loader>(
                    nt_shared_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::shared_writeback, false) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::nt_file_loader>(
                    nt_shared_readonly_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::read_only, true) == 0);

                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::zw_file_loader>(
                    nt_private_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::private_copy, true) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::zw_file_loader>(
                    nt_private_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::private_copy, false) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::zw_file_loader>(
                    nt_shared_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::shared_writeback, false) == 0);
                UWVM_TEST_REQUIRE(exercise_loader_matrix<::fast_io::zw_file_loader>(
                    nt_shared_readonly_options, sizes, sizeof(sizes) / sizeof(*sizes), page_size, extra_size,
                    padding_mode, mapped_write_semantics::read_only, true) == 0);
            }
        }
        return 0;
    }
} // namespace uwvm2_test_windows_file_loader_padding

int main()
{
    try
    {
        return ::uwvm2_test_windows_file_loader_padding::run();
    }
    catch(::fast_io::error e)
    {
        ::std::fprintf(stderr, "fast_io::error domain=%zu code=%zu payload=%zu extra=%zu semantics=%u readonly=%u\n",
                       e.domain, e.code,
                       ::uwvm2_test_windows_file_loader_padding::current_payload_size,
                       ::uwvm2_test_windows_file_loader_padding::current_extra_size,
                       static_cast<unsigned>(::uwvm2_test_windows_file_loader_padding::current_semantics),
                       ::uwvm2_test_windows_file_loader_padding::current_readonly_file ? 1u : 0u);
        return 255;
    }
    catch(...)
    {
        ::std::fprintf(stderr, "unknown exception payload=%zu extra=%zu semantics=%u readonly=%u\n",
                       ::uwvm2_test_windows_file_loader_padding::current_payload_size,
                       ::uwvm2_test_windows_file_loader_padding::current_extra_size,
                       static_cast<unsigned>(::uwvm2_test_windows_file_loader_padding::current_semantics),
                       ::uwvm2_test_windows_file_loader_padding::current_readonly_file ? 1u : 0u);
        return 255;
    }
}

#else

int main()
{
    return 0;
}

#endif

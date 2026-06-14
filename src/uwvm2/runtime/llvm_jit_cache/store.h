/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-14
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

#pragma once

#ifndef UWVM_MODULE
// std
# include <algorithm>
# include <cerrno>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include "format.h"
# include "compress.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    namespace details
    {
        using sha256_digest = ::uwvm2::utils::container::array<::std::byte, ::uwvm2::runtime::llvm_jit_cache::cache_hmac_sha256_size>;

        inline constexpr void append_hex_byte(::uwvm2::utils::container::u8string& out, ::std::byte b) noexcept
        {
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, ::fast_io::mnp::hex<false, true>(::std::to_integer<::std::uint_least8_t>(b)));
        }

        inline constexpr void sha256_update(::fast_io::sha256_context& sha, ::std::byte const* first, ::std::byte const* last) noexcept
        {
            if(first != last) { sha.update(first, last); }
        }

        inline constexpr void sha256_update(::fast_io::sha256_context& sha, ::uwvm2::utils::container::u8string_view v) noexcept
        { sha256_update(sha, reinterpret_cast<::std::byte const*>(v.cbegin()), reinterpret_cast<::std::byte const*>(v.cend())); }

        [[nodiscard]] inline constexpr sha256_digest sha256_bytes(::std::byte const* first, ::std::byte const* last) noexcept
        {
            ::fast_io::sha256_context sha{};
            sha256_update(sha, first, last);
            sha.do_final();
            sha256_digest out{};
            sha.digest_to_byte_ptr(out.data());
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            sha256_hex_for_path(::uwvm2::utils::container::vector<::std::byte> const& bytes) noexcept
        {
            auto const digest{sha256_bytes(bytes.cbegin(), bytes.cend())};
            ::uwvm2::utils::container::u8string out{};
            out.reserve(::uwvm2::runtime::llvm_jit_cache::cache_hmac_sha256_size * 2uz);
            for(auto b: digest) { append_hex_byte(out, b); }
            return out;
        }

        [[nodiscard]] inline constexpr sha256_digest hmac_sha256_identity(cache_context const& ctx,
                                                                          ::uwvm2::utils::container::vector<::std::byte> const& header,
                                                                          ::uwvm2::utils::container::vector<::std::byte> const& isa_metadata,
                                                                          ::uwvm2::utils::container::vector<::std::byte> const& context_metadata,
                                                                          ::uwvm2::utils::container::vector<::std::byte> const& payload) noexcept
        {
            constexpr ::std::size_t block_size{64uz};
            ::uwvm2::utils::container::array<::std::byte, block_size> key_block{};

            auto const identity_first{reinterpret_cast<::std::byte const*>(ctx.identity.cbegin())};
            auto const identity_last{reinterpret_cast<::std::byte const*>(ctx.identity.cend())};
            auto const ctx_identity_size{ctx.identity.size()};
            if(ctx_identity_size > block_size)
            {
                auto const hashed{sha256_bytes(identity_first, identity_last)};
                ::std::copy(hashed.begin(), hashed.end(), key_block.begin());
            }
            else if(ctx_identity_size != 0uz) { ::std::copy(identity_first, identity_last, key_block.begin()); }

            ::uwvm2::utils::container::array<::std::byte, block_size> ipad{};
            ::uwvm2::utils::container::array<::std::byte, block_size> opad{};
            for(::std::size_t i{}; i != block_size; ++i)
            {
                ipad[i] = static_cast<::std::byte>(::std::to_integer<unsigned>(key_block[i]) ^ 0x36u);
                opad[i] = static_cast<::std::byte>(::std::to_integer<unsigned>(key_block[i]) ^ 0x5cu);
            }

            ::fast_io::sha256_context inner{};
            sha256_update(inner, ipad.cbegin(), ipad.cend());
            sha256_update(inner, header.cbegin(), header.cend());
            sha256_update(inner, isa_metadata.cbegin(), isa_metadata.cend());
            sha256_update(inner, context_metadata.cbegin(), context_metadata.cend());
            sha256_update(inner, payload.cbegin(), payload.cend());
            inner.do_final();
            sha256_digest inner_digest{};
            inner.digest_to_byte_ptr(inner_digest.data());

            ::fast_io::sha256_context outer{};
            sha256_update(outer, opad.cbegin(), opad.cend());
            sha256_update(outer, inner_digest.cbegin(), inner_digest.cend());
            outer.do_final();
            sha256_digest out{};
            outer.digest_to_byte_ptr(out.data());
            return out;
        }

        [[nodiscard]] inline constexpr bool constant_time_equal(::std::byte const* first, ::std::byte const* last, ::std::byte const* expected) noexcept
        {
            unsigned diff{};
            for(; first != last; ++first, ++expected) { diff |= ::std::to_integer<unsigned>(*first) ^ ::std::to_integer<unsigned>(*expected); }
            return diff == 0u;
        }

        [[nodiscard]] inline constexpr bool cache_path_separator(char8_t ch) noexcept
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            return ch == u8'/' || ch == u8'\\';
#else
            return ch == u8'/';
#endif
        }

        inline constexpr void append_path_separator_if_needed(::uwvm2::utils::container::u8string& path) noexcept
        {
            if(path.empty()) { return; }
            auto const last{path.back_unchecked()};
            if(!cache_path_separator(last))
            {
                ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(path)};
                ::fast_io::io::print(ref, u8'/');
            }
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            cache_path_piece(::uwvm2::utils::container::u8string_view path, ::std::size_t first, ::std::size_t last) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            out.reserve(last - first);
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, path.subview_unchecked(first, last - first));
            return out;
        }

        inline constexpr void append_cache_path_component(::uwvm2::utils::container::u8string& path,
                                                          ::uwvm2::utils::container::u8string const& component) noexcept
        {
            append_path_separator_if_needed(path);
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(path)};
            ::fast_io::io::print(ref, component);
        }

        struct cache_dir_cursor
        {
            ::fast_io::dir_file dir{};
            ::std::size_t next{};
            ::uwvm2::utils::container::u8string display_path{};

            inline constexpr cache_dir_cursor() noexcept = default;
            cache_dir_cursor(cache_dir_cursor const&) = delete;
            cache_dir_cursor& operator= (cache_dir_cursor const&) = delete;
            inline constexpr cache_dir_cursor(cache_dir_cursor&&) noexcept = default;
            inline constexpr cache_dir_cursor& operator= (cache_dir_cursor&&) noexcept = default;

            inline constexpr cache_dir_cursor(::fast_io::dir_file&& directory,
                                              ::std::size_t next_pos,
                                              ::uwvm2::utils::container::u8string&& display) noexcept :
                dir{::std::move(directory)}, next{next_pos}, display_path{::std::move(display)}
            {
            }
        };

        [[nodiscard]] inline constexpr cache_dir_cursor open_cache_dir_root(::uwvm2::utils::container::u8string_view path) UWVM_THROWS
        {
            if(path.empty())
            {
                return {
                    ::fast_io::dir_file{u8".", ::fast_io::open_mode::follow},
                    0uz,
                    {}
                };
            }

#if defined(_WIN32) && !defined(__CYGWIN__)
            if(path.size() >= 2uz && cache_path_separator(path.index_unchecked(0uz)) && cache_path_separator(path.index_unchecked(1uz)))
            {
                ::std::size_t pos{2uz};
                while(pos != path.size() && cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                while(pos != path.size() && !cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                while(pos != path.size() && cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                while(pos != path.size() && !cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                auto root{cache_path_piece(path, 0uz, pos)};
                while(pos != path.size() && cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                return {
                    ::fast_io::dir_file{root, ::fast_io::open_mode::follow},
                    pos,
                    ::std::move(root)
                };
            }

            if(path.size() >= 3uz && path.index_unchecked(1uz) == u8':' && cache_path_separator(path.index_unchecked(2uz)))
            {
                auto root{cache_path_piece(path, 0uz, 3uz)};
                ::std::size_t pos{3uz};
                while(pos != path.size() && cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                return {
                    ::fast_io::dir_file{root, ::fast_io::open_mode::follow},
                    pos,
                    ::std::move(root)
                };
            }

            if(path.size() >= 2uz && path.index_unchecked(1uz) == u8':')
            {
                auto root{cache_path_piece(path, 0uz, 2uz)};
                return {
                    ::fast_io::dir_file{root, ::fast_io::open_mode::follow},
                    2uz,
                    ::std::move(root)
                };
            }
#endif

            if(cache_path_separator(path.index_unchecked(0uz)))
            {
                auto root{cache_path_piece(path, 0uz, 1uz)};
                ::std::size_t pos{1uz};
                while(pos != path.size() && cache_path_separator(path.index_unchecked(pos))) { ++pos; }
                return {
                    ::fast_io::dir_file{root, ::fast_io::open_mode::follow},
                    pos,
                    ::std::move(root)
                };
            }

            return {
                ::fast_io::dir_file{u8".", ::fast_io::open_mode::follow},
                0uz,
                {}
            };
        }

        [[nodiscard]] inline constexpr bool mkdir_error_is_file_exists(::fast_io::error e) noexcept
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            // windows
            switch(e.domain)
            {
                // win32
                case ::fast_io::win32_domain_value:
                {
                    return e.code == 183uz /*ERROR_ALREADY_EXISTS*/ || e.code == 80uz /*ERROR_FILE_EXISTS*/;
                }
# if !defined(_WIN32_WINDOWS)
                // winnt
                case ::fast_io::nt_domain_value:
                {
                    static_assert(sizeof(::fast_io::error::value_type) >= sizeof(::std::uint_least32_t));
                    return e.code == 0xC0000035uz /*STATUS_OBJECT_NAME_COLLISION*/;
                }
# endif
# if defined(EEXIST)
                // posix
                [[unlikely]] case ::fast_io::posix_domain_value:
                {
                    return e.code == static_cast<::fast_io::error::value_type>(static_cast<unsigned>(EEXIST));
                }
# endif
                [[unlikely]] default:
                {
                    return false;
                }
            }
#else
            // POSIX
# if defined(EEXIST)
            return e.domain == ::fast_io::posix_domain_value && e.code == static_cast<::fast_io::error::value_type>(static_cast<unsigned>(EEXIST));
# else
            (void)e;
            return false;
# endif
#endif
        }

        inline constexpr void warn_make_directory_failed([[maybe_unused]] ::uwvm2::utils::container::u8string const& path,
                                                         [[maybe_unused]] ::fast_io::error e) noexcept
        {
#if defined(UWVM)
            if(!::uwvm2::uwvm::io::show_runtime_warning) { return; }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Failed to create LLVM JIT cache directory \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                path,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". error: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                e,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8" (runtime)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            if(::uwvm2::uwvm::io::runtime_warning_fatal) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime)\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }
#endif
        }

        inline constexpr void try_make_directory_at(::fast_io::dir_file& parent,
                                                    ::uwvm2::utils::container::u8string const& component,
                                                    ::uwvm2::utils::container::u8string const& display_path) noexcept
        {
            if(component.empty()) { return; }

#ifdef UWVM_CPP_EXCEPTIONS
            try
#endif
            {
                ::fast_io::native_mkdirat(::fast_io::at(parent), component);
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                if(mkdir_error_is_file_exists(e)) [[likely]] { return; }
                warn_make_directory_failed(display_path, e);
            }
#endif
        }

        [[nodiscard]] inline constexpr ::fast_io::dir_file open_cache_dir(::uwvm2::utils::container::u8string_view dir, bool create) UWVM_THROWS
        {
            auto cursor{open_cache_dir_root(dir)};
            ::uwvm2::utils::container::u8string component{};
            component.reserve(dir.size());
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(component)};

            for(::std::size_t i{cursor.next}; i != dir.size();)
            {
                while(i != dir.size() && cache_path_separator(dir.index_unchecked(i))) { ++i; }
                if(i == dir.size()) { break; }

                auto const first{i};
                while(i != dir.size() && !cache_path_separator(dir.index_unchecked(i))) { ++i; }

                component.clear();
                ::fast_io::io::print(ref, dir.subview_unchecked(first, i - first));
                append_cache_path_component(cursor.display_path, component);
                if(create) { try_make_directory_at(cursor.dir, component, cursor.display_path); }
                cursor.dir = ::fast_io::dir_file{::fast_io::at(cursor.dir), component, ::fast_io::open_mode::follow};
            }

            return ::std::move(cursor.dir);
        }

        [[nodiscard]] inline constexpr ::fast_io::dir_file make_directories_for_cache_dir(::uwvm2::utils::container::u8string_view dir) UWVM_THROWS
        { return open_cache_dir(dir, true); }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> cache_path_key_bytes(cache_context const& ctx) noexcept
        {
            auto isa{make_isa_metadata(ctx)};
            auto context{make_context_metadata(ctx)};
            ::uwvm2::utils::container::vector<::std::byte> bytes{};
            bytes.reserve(ctx.cache_key.size() + isa.size() + context.size() + 16uz);
            append_u8string_bytes(bytes, ctx.cache_key);
            bytes.push_back(static_cast<::std::byte>('\n'));
            append_bytes(bytes, isa.cbegin(), isa.cend());
            append_bytes(bytes, context.cbegin(), context.cend());
            return bytes;
        }

        [[nodiscard]] inline constexpr bool valid_signature_shape(cache_blob_view const& view) noexcept
        {
            if(view.header.signature == static_cast<::std::uint_least32_t>(signature_kind::none)) { return view.header.signature_size == 0u; }
            if(view.header.signature == static_cast<::std::uint_least32_t>(signature_kind::hmac_sha256_identity))
            {
                return view.header.signature_size == cache_hmac_sha256_size;
            }
            return true;
        }
    }  // namespace details

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_file_name(cache_context const& ctx) noexcept
    {
        auto const key_bytes{details::cache_path_key_bytes(ctx)};
        auto name{details::sha256_hex_for_path(key_bytes)};
        name.reserve(name.size() + 9uz);
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(name)};
        ::fast_io::io::print(ref, u8".uwvm-ljc");
        return name;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_file_path(cache_context const& ctx) noexcept
    {
        auto path{ctx.cache_dir};
        details::append_path_separator_if_needed(path);
        auto const name{cache_file_name(ctx)};
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(path)};
        ::fast_io::io::print(ref, name);
        return path;
    }

    [[nodiscard]] inline constexpr cache_status store_object(cache_context const& ctx,
                                                             ::std::byte const* object,
                                                             ::std::size_t size,
                                                             cache_policy const& policy) noexcept
    {
        if(!policy.enable) { return cache_status::disabled; }
        if(object == nullptr && size != 0uz) [[unlikely]] { return cache_status::malformed; }
        if(size > policy.max_object_bytes) [[unlikely]] { return cache_status::size_limit_exceeded; }

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            ::uwvm2::utils::container::vector<::std::byte> payload{};
            auto compression{policy.compression};
            switch(policy.compression)
            {
                case compression_kind::none:
                    payload.reserve(size);
                    details::append_bytes(payload, object, object + size);
                    break;
                case compression_kind::uwvm_lzss: payload = compress_lzss(object, size); break;
                case compression_kind::uwvm_native_lz: payload = compress_native_lz(object, size); break;
                default: return cache_status::unsupported_compression;
            }

            if(compression != compression_kind::none && payload.size() >= size)
            {
                compression = compression_kind::none;
                payload = {};
                payload.reserve(size);
                details::append_bytes(payload, object, object + size);
            }

            auto isa_metadata{make_isa_metadata(ctx)};
            auto context_metadata{make_context_metadata(ctx)};

            cache_fixed_header header{};
            ::std::copy(cache_magic, cache_magic + 8uz, header.magic);
            header.version = cache_format_version;
            header.fixed_header_size = static_cast<::std::uint_least32_t>(cache_fixed_header_size);
            header.compression = static_cast<::std::uint_least32_t>(compression);
            header.signature = policy.generate_signature ? static_cast<::std::uint_least32_t>(signature_kind::hmac_sha256_identity)
                                                         : static_cast<::std::uint_least32_t>(signature_kind::none);
            header.uncompressed_size = static_cast<::std::uint_least64_t>(size);
            header.payload_size = static_cast<::std::uint_least64_t>(payload.size());
            header.isa_metadata_size = static_cast<::std::uint_least64_t>(isa_metadata.size());
            header.context_metadata_size = static_cast<::std::uint_least64_t>(context_metadata.size());
            header.signature_size = policy.generate_signature ? cache_hmac_sha256_size : 0uz;

            auto header_bytes{serialize_fixed_header(header)};
            ::uwvm2::utils::container::vector<::std::byte> signature{};
            if(policy.generate_signature)
            {
                auto const digest{details::hmac_sha256_identity(ctx, header_bytes, isa_metadata, context_metadata, payload)};
                signature.reserve(digest.size());
                details::append_bytes(signature, digest.cbegin(), digest.cend());
            }

            ::uwvm2::utils::container::vector<::std::byte> blob{};
            blob.reserve(header_bytes.size() + isa_metadata.size() + context_metadata.size() + signature.size() + payload.size());
            details::append_bytes(blob, header_bytes.cbegin(), header_bytes.cend());
            details::append_bytes(blob, isa_metadata.cbegin(), isa_metadata.cend());
            details::append_bytes(blob, context_metadata.cbegin(), context_metadata.cend());
            details::append_bytes(blob, signature.cbegin(), signature.cend());
            details::append_bytes(blob, payload.cbegin(), payload.cend());

            auto cache_dir{details::make_directories_for_cache_dir(::uwvm2::utils::container::u8string_view{ctx.cache_dir.cbegin(), ctx.cache_dir.size()})};
            auto const file_name{cache_file_name(ctx)};
            ::fast_io::u8obuf_file file{::fast_io::at(cache_dir),
                                        file_name,
                                        ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
            ::fast_io::operations::write_all_bytes(file, blob.cbegin(), blob.cend());
            return cache_status::ok;
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            return cache_status::io_error;
        }
#endif
    }

    [[nodiscard]] inline constexpr cache_load_result load_object(cache_context const& ctx, cache_policy const& policy) noexcept
    {
        cache_load_result result{};

        if(!policy.enable)
        {
            result.status = cache_status::disabled;
            return result;
        }

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            auto cache_dir{details::open_cache_dir(::uwvm2::utils::container::u8string_view{ctx.cache_dir.cbegin(), ctx.cache_dir.size()}, false)};
            auto const file_name{cache_file_name(ctx)};
            ::fast_io::native_file_loader file{::fast_io::at(cache_dir), file_name, ::fast_io::open_mode::in | ::fast_io::open_mode::follow};
            auto const first{reinterpret_cast<::std::byte const*>(file.cbegin())};
            auto const last{reinterpret_cast<::std::byte const*>(file.cend())};

            cache_blob_view view{};
            if(auto const status{parse_cache_blob(first, last, view)}; status != cache_status::ok)
            {
                result.status = status;
                return result;
            }

            if(!details::valid_signature_shape(view))
            {
                result.status = cache_status::malformed;
                return result;
            }

            if(view.header.uncompressed_size > static_cast<::std::uint_least64_t>(policy.max_object_bytes))
            {
                result.status = cache_status::size_limit_exceeded;
                return result;
            }

            auto expected_isa{make_isa_metadata(ctx)};
            if(!metadata_equal(view.isa_metadata, view.header.isa_metadata_size, expected_isa))
            {
                result.status = cache_status::isa_mismatch;
                return result;
            }
            result.isa_matched = true;

            auto expected_context{make_context_metadata(ctx)};
            if(!metadata_equal(view.context_metadata, view.header.context_metadata_size, expected_context))
            {
                result.status = cache_status::context_mismatch;
                return result;
            }

            auto const compression{static_cast<compression_kind>(view.header.compression)};
            if(compression != compression_kind::none && compression != compression_kind::uwvm_lzss && compression != compression_kind::uwvm_native_lz)
            {
                result.status = cache_status::unsupported_compression;
                return result;
            }

            if(policy.verify_signature)
            {
                if(view.header.signature == static_cast<::std::uint_least32_t>(signature_kind::none))
                {
                    result.status = cache_status::signature_missing;
                    return result;
                }
                if(view.header.signature != static_cast<::std::uint_least32_t>(signature_kind::hmac_sha256_identity))
                {
                    result.status = cache_status::unsupported_signature;
                    return result;
                }

                auto header_bytes{serialize_fixed_header(view.header)};
                auto const isa_size{static_cast<::std::size_t>(view.header.isa_metadata_size)};
                auto const context_size{static_cast<::std::size_t>(view.header.context_metadata_size)};
                auto const payload_size{static_cast<::std::size_t>(view.header.payload_size)};

                ::uwvm2::utils::container::vector<::std::byte> isa_metadata{};
                isa_metadata.reserve(isa_size);
                details::append_bytes(isa_metadata, view.isa_metadata, view.isa_metadata + isa_size);

                ::uwvm2::utils::container::vector<::std::byte> context_metadata{};
                context_metadata.reserve(context_size);
                details::append_bytes(context_metadata, view.context_metadata, view.context_metadata + context_size);

                ::uwvm2::utils::container::vector<::std::byte> payload{};
                payload.reserve(payload_size);
                details::append_bytes(payload, view.payload, view.payload + payload_size);

                auto const digest{details::hmac_sha256_identity(ctx, header_bytes, isa_metadata, context_metadata, payload)};
                if(!details::constant_time_equal(view.signature, view.signature + cache_hmac_sha256_size, digest.data()))
                {
                    result.status = cache_status::signature_mismatch;
                    return result;
                }
                result.signature_verified = true;
            }

            auto const uncompressed_size{static_cast<::std::size_t>(view.header.uncompressed_size)};
            auto const payload_size{static_cast<::std::size_t>(view.header.payload_size)};
            if(!decompress_payload(compression, view.payload, payload_size, uncompressed_size, result.object))
            {
                result.status = cache_status::decompression_failed;
                result.object = {};
                return result;
            }

            result.status = cache_status::ok;
            return result;
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            result.status = cache_status::io_error;
            return result;
        }
#endif
    }
}  // namespace uwvm2::runtime::llvm_jit_cache

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

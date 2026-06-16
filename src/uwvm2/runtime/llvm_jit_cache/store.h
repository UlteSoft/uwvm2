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
# include <atomic>
# include <cerrno>
# include <chrono>
# include <condition_variable>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <deque>
# include <limits>
# include <memory>
# include <mutex>
# include <thread>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
#  include <unistd.h>
# endif
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include "format.h"
# include "compress.h"
# if !defined(UWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519)
#  error "LLVM JIT cache Ed25519 signatures require OpenSSL."
# endif
# include <openssl/evp.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    namespace details
    {
        using sha256_digest = ::uwvm2::utils::container::array<::std::byte, ::uwvm2::runtime::llvm_jit_cache::cache_sha256_digest_size>;

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
            out.reserve(::uwvm2::runtime::llvm_jit_cache::cache_sha256_digest_size * 2uz);
            for(auto b: digest) { append_hex_byte(out, b); }
            return out;
        }

        inline constexpr void append_bytes_n(::uwvm2::utils::container::vector<::std::byte>& out, ::std::byte const* first, ::std::size_t size) noexcept
        {
            if(size != 0uz) { append_bytes(out, first, first + size); }
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte>
            ed25519_signature_message_parts(::uwvm2::utils::container::vector<::std::byte> const& header,
                                            ::std::byte const* isa_metadata,
                                            ::std::size_t isa_metadata_size,
                                            ::std::byte const* context_metadata,
                                            ::std::size_t context_metadata_size,
                                            ::std::byte const* payload,
                                            ::std::size_t payload_size) noexcept
        {
            ::uwvm2::utils::container::vector<::std::byte> message{};
            message.reserve(header.size() + isa_metadata_size + context_metadata_size + payload_size);
            append_bytes_n(message, header.cbegin(), header.size());
            append_bytes_n(message, isa_metadata, isa_metadata_size);
            append_bytes_n(message, context_metadata, context_metadata_size);
            append_bytes_n(message, payload, payload_size);
            return message;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte>
            ed25519_signature_message(::uwvm2::utils::container::vector<::std::byte> const& header,
                                      ::uwvm2::utils::container::vector<::std::byte> const& isa_metadata,
                                      ::uwvm2::utils::container::vector<::std::byte> const& context_metadata,
                                      ::uwvm2::utils::container::vector<::std::byte> const& payload) noexcept
        {
            return ed25519_signature_message_parts(header,
                                                   isa_metadata.cbegin(),
                                                   isa_metadata.size(),
                                                   context_metadata.cbegin(),
                                                   context_metadata.size(),
                                                   payload.cbegin(),
                                                   payload.size());
        }

        [[nodiscard]] inline constexpr bool ed25519_identity_sign(cache_context const& ctx,
                                                                  ::uwvm2::utils::container::vector<::std::byte> const& header,
                                                                  ::uwvm2::utils::container::vector<::std::byte> const& isa_metadata,
                                                                  ::uwvm2::utils::container::vector<::std::byte> const& context_metadata,
                                                                  ::uwvm2::utils::container::vector<::std::byte> const& payload,
                                                                  ::uwvm2::utils::container::vector<::std::byte>& signature) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519)
            if(!ctx.has_signature_seed) { return false; }
            auto const message{ed25519_signature_message(header, isa_metadata, context_metadata, payload)};
            auto const seed{reinterpret_cast<unsigned char const*>(ctx.signature_seed.data())};
            auto private_key{::EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, cache_ed25519_seed_size)};
            if(private_key == nullptr) { return false; }

            auto md_ctx{::EVP_MD_CTX_new()};
            if(md_ctx == nullptr)
            {
                ::EVP_PKEY_free(private_key);
                return false;
            }

            ::std::size_t signature_size{cache_ed25519_signature_size};
            signature.resize(signature_size);
            auto const ok{::EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, private_key) == 1 &&
                          ::EVP_DigestSign(md_ctx,
                                           reinterpret_cast<unsigned char*>(signature.data()),
                                           ::std::addressof(signature_size),
                                           reinterpret_cast<unsigned char const*>(message.data()),
                                           message.size()) == 1};
            ::EVP_MD_CTX_free(md_ctx);
            ::EVP_PKEY_free(private_key);
            if(!ok) { return false; }
            signature.resize(signature_size);
            return signature_size == cache_ed25519_signature_size;
#else
# error "LLVM JIT cache Ed25519 signatures require OpenSSL."
#endif
        }

        [[nodiscard]] inline constexpr bool ed25519_identity_verify(cache_context const& ctx,
                                                                    ::uwvm2::utils::container::vector<::std::byte> const& header,
                                                                    ::std::byte const* isa_metadata,
                                                                    ::std::size_t isa_metadata_size,
                                                                    ::std::byte const* context_metadata,
                                                                    ::std::size_t context_metadata_size,
                                                                    ::std::byte const* signature,
                                                                    ::std::byte const* payload,
                                                                    ::std::size_t payload_size) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519)
            if(!ctx.has_signature_seed || signature == nullptr) { return false; }
            auto const message{ed25519_signature_message_parts(
                header, isa_metadata, isa_metadata_size, context_metadata, context_metadata_size, payload, payload_size)};
            auto const seed{reinterpret_cast<unsigned char const*>(ctx.signature_seed.data())};
            auto private_key{::EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, cache_ed25519_seed_size)};
            if(private_key == nullptr) { return false; }

            ::uwvm2::utils::container::array<::std::byte, 32uz> public_key{};
            ::std::size_t public_key_size{public_key.size()};
            auto const public_key_ok{
                ::EVP_PKEY_get_raw_public_key(private_key, reinterpret_cast<unsigned char*>(public_key.data()), ::std::addressof(public_key_size)) == 1 &&
                public_key_size == public_key.size()};
            ::EVP_PKEY_free(private_key);
            if(!public_key_ok) { return false; }

            auto verify_key{
                ::EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, reinterpret_cast<unsigned char const*>(public_key.data()), public_key.size())};
            if(verify_key == nullptr) { return false; }

            auto md_ctx{::EVP_MD_CTX_new()};
            if(md_ctx == nullptr)
            {
                ::EVP_PKEY_free(verify_key);
                return false;
            }

            auto const ok{::EVP_DigestVerifyInit(md_ctx, nullptr, nullptr, nullptr, verify_key) == 1 &&
                          ::EVP_DigestVerify(md_ctx,
                                             reinterpret_cast<unsigned char const*>(signature),
                                             cache_ed25519_signature_size,
                                             reinterpret_cast<unsigned char const*>(message.data()),
                                             message.size()) == 1};
            ::EVP_MD_CTX_free(md_ctx);
            ::EVP_PKEY_free(verify_key);
            return ok;
#else
# error "LLVM JIT cache Ed25519 signatures require OpenSSL."
#endif
        }

        [[nodiscard]] inline constexpr bool ed25519_identity_verify(cache_context const& ctx,
                                                                    ::uwvm2::utils::container::vector<::std::byte> const& header,
                                                                    ::uwvm2::utils::container::vector<::std::byte> const& isa_metadata,
                                                                    ::uwvm2::utils::container::vector<::std::byte> const& context_metadata,
                                                                    ::std::byte const* signature,
                                                                    ::uwvm2::utils::container::vector<::std::byte> const& payload) noexcept
        {
            return ed25519_identity_verify(ctx,
                                           header,
                                           isa_metadata.cbegin(),
                                           isa_metadata.size(),
                                           context_metadata.cbegin(),
                                           context_metadata.size(),
                                           signature,
                                           payload.cbegin(),
                                           payload.size());
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
                ::fast_io::io::print(ref, u8"/");
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

            inline constexpr cache_dir_cursor(::fast_io::dir_file&& directory, ::std::size_t next_pos, ::uwvm2::utils::container::u8string&& display) noexcept :
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

        inline constexpr void warn_cache_write_failed([[maybe_unused]] ::uwvm2::utils::container::u8string const& cache_dir,
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
                                u8"Failed to write LLVM JIT cache object \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                cache_dir,
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
            bytes.reserve(ctx.cache_key.size() + isa.size() + context.size() + 48uz);
            append_key_value(bytes, u8"format", u8"uwvm-ljc-path-key-v1");
            append_key_value(bytes, u8"cache-key", ctx.cache_key);
            append_key_value(bytes, u8"isa", isa);
            append_key_value(bytes, u8"context", context);
            return bytes;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_key_hash(cache_context const& ctx) noexcept
        {
            auto const key_bytes{cache_path_key_bytes(ctx)};
            return sha256_hex_for_path(key_bytes);
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            cache_file_name_from_hash(::uwvm2::utils::container::u8string const& key_hash) noexcept
        {
            auto name{key_hash};
            name.reserve(name.size() + 9uz);
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(name)};
            ::fast_io::io::print(ref, u8".uwvm-ljc");
            return name;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_key_shard(::uwvm2::utils::container::u8string const& key_hash) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, key_hash.subview_unchecked(0uz, 2uz));
            return out;
        }

        [[nodiscard]] inline constexpr ::fast_io::dir_file
            open_cache_object_dir(cache_context const& ctx, ::uwvm2::utils::container::u8string const& key_hash, bool create) UWVM_THROWS
        {
            auto root{open_cache_dir(::uwvm2::utils::container::u8string_view{ctx.cache_dir.cbegin(), ctx.cache_dir.size()}, create)};

            ::uwvm2::utils::container::u8string objects_component{u8"objects"};
            ::uwvm2::utils::container::u8string display_path{ctx.cache_dir};
            append_cache_path_component(display_path, objects_component);
            if(create) { try_make_directory_at(root, objects_component, display_path); }
            auto objects_dir{
                ::fast_io::dir_file{::fast_io::at(root), objects_component, ::fast_io::open_mode::follow}
            };

            auto shard_component{cache_key_shard(key_hash)};
            append_cache_path_component(display_path, shard_component);
            if(create) { try_make_directory_at(objects_dir, shard_component, display_path); }
            return ::fast_io::dir_file{::fast_io::at(objects_dir), shard_component, ::fast_io::open_mode::follow};
        }

        [[nodiscard]] inline constexpr ::std::uint_least64_t cache_process_id() noexcept
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            return static_cast<::std::uint_least64_t>(::fast_io::win32::GetCurrentProcessId());
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
            return static_cast<::std::uint_least64_t>(::getpid());
#else
            return 0u;
#endif
        }

        inline ::std::atomic_uint_least64_t cache_atomic_write_counter{};  // [global]

        [[nodiscard]] inline constexpr ::std::uint_least64_t cache_atomic_write_nonce() noexcept
        {
            auto const ticks{static_cast<::std::uint_least64_t>(::std::chrono::steady_clock::now().time_since_epoch().count())};
            auto const address{static_cast<::std::uint_least64_t>(reinterpret_cast<::std::uintptr_t>(::std::addressof(cache_atomic_write_counter)))};
            auto const pid{cache_process_id()};
            return ticks ^ (address << 1u) ^ (pid << 32u) ^ pid;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            cache_atomic_temp_file_name(::uwvm2::utils::container::u8string const& file_name) noexcept
        {
            static auto const nonce{cache_atomic_write_nonce()};  // [global]
            auto const counter{cache_atomic_write_counter.fetch_add(1u, ::std::memory_order_relaxed)};
            auto temp_name{file_name};
            temp_name.reserve(temp_name.size() + 64uz);
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(temp_name)};
            ::fast_io::io::print(ref, u8".wip-atomic-write-", ::fast_io::mnp::hex<false, true>(nonce), u8"-", counter);
            return temp_name;
        }

        inline constexpr cache_status write_cache_blob_atomic(cache_context const& ctx, ::uwvm2::utils::container::vector<::std::byte> const& blob) noexcept
        {
#ifdef UWVM_CPP_EXCEPTIONS
            try
#endif
            {
                auto const key_hash{cache_key_hash(ctx)};
                auto cache_dir{open_cache_object_dir(ctx, key_hash, true)};
                auto const file_name{cache_file_name_from_hash(key_hash)};
                auto const temp_name{cache_atomic_temp_file_name(file_name)};

                {
                    ::fast_io::u8obuf_file file{::fast_io::at(cache_dir),
                                                temp_name,
                                                ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::excl};
                    ::fast_io::operations::write_all_bytes(file, blob.cbegin(), blob.cend());
                }

                ::fast_io::native_renameat(::fast_io::at(cache_dir), temp_name, ::fast_io::at(cache_dir), file_name);
                return cache_status::ok;
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                warn_cache_write_failed(ctx.cache_dir, e);
                return cache_status::io_error;
            }
#endif
        }

        inline constexpr cache_status build_cache_blob(cache_context const& ctx,
                                                       ::std::byte const* object,
                                                       ::std::size_t size,
                                                       cache_policy const& policy,
                                                       ::uwvm2::utils::container::vector<::std::byte>& blob) noexcept
        {
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
                        if(size != 0uz) { append_bytes(payload, object, object + size); }
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
                    if(size != 0uz) { append_bytes(payload, object, object + size); }
                }

                auto isa_metadata{make_isa_metadata(ctx)};
                auto context_metadata{make_context_metadata(ctx)};

                cache_fixed_header header{};
                ::std::copy(cache_magic, cache_magic + 8uz, header.magic);
                header.version = cache_format_version;
                header.fixed_header_size = static_cast<::std::uint_least32_t>(cache_fixed_header_size);
                header.compression = static_cast<::std::uint_least32_t>(compression);
                header.signature = policy.generate_signature ? static_cast<::std::uint_least32_t>(signature_kind::ed25519_identity)
                                                             : static_cast<::std::uint_least32_t>(signature_kind::none);
                header.uncompressed_size = static_cast<::std::uint_least64_t>(size);
                header.payload_size = static_cast<::std::uint_least64_t>(payload.size());
                header.isa_metadata_size = static_cast<::std::uint_least64_t>(isa_metadata.size());
                header.context_metadata_size = static_cast<::std::uint_least64_t>(context_metadata.size());
                header.signature_size = policy.generate_signature ? cache_ed25519_signature_size : 0uz;

                auto header_bytes{serialize_fixed_header(header)};
                ::uwvm2::utils::container::vector<::std::byte> signature{};
                if(policy.generate_signature)
                {
                    if(!cache_ed25519_identity_signature_available) { return cache_status::unsupported_signature; }
                    signature.reserve(cache_ed25519_signature_size);
                    if(!ed25519_identity_sign(ctx, header_bytes, isa_metadata, context_metadata, payload, signature))
                    {
                        return cache_status::unsupported_signature;
                    }
                    if(signature.size() != cache_ed25519_signature_size) [[unlikely]] { return cache_status::unsupported_signature; }
                }

                blob = {};
                blob.reserve(header_bytes.size() + isa_metadata.size() + context_metadata.size() + signature.size() + payload.size());
                append_bytes(blob, header_bytes.cbegin(), header_bytes.cend());
                append_bytes(blob, isa_metadata.cbegin(), isa_metadata.cend());
                append_bytes(blob, context_metadata.cbegin(), context_metadata.cend());
                append_bytes(blob, signature.cbegin(), signature.cend());
                append_bytes(blob, payload.cbegin(), payload.cend());
                return cache_status::ok;
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                static_cast<void>(e);
                return cache_status::io_error;
            }
#endif
        }

        [[nodiscard]] inline constexpr bool valid_signature_shape(cache_blob_view const& view) noexcept
        {
            if(view.header.signature == static_cast<::std::uint_least32_t>(signature_kind::none)) { return view.header.signature_size == 0u; }
            if(view.header.signature == static_cast<::std::uint_least32_t>(signature_kind::ed25519_identity))
            {
                return view.header.signature_size == cache_ed25519_signature_size;
            }
            return true;
        }
    }  // namespace details

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_file_name(cache_context const& ctx) noexcept
    { return details::cache_file_name_from_hash(details::cache_key_hash(ctx)); }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_file_path(cache_context const& ctx) noexcept
    {
        auto path{ctx.cache_dir};
        details::append_path_separator_if_needed(path);
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(path)};
        auto const key_hash{details::cache_key_hash(ctx)};
        auto const shard{details::cache_key_shard(key_hash)};
        ::fast_io::io::print(ref, u8"objects");
        details::append_path_separator_if_needed(path);
        ::fast_io::io::print(ref, shard);
        details::append_path_separator_if_needed(path);
        auto const name{details::cache_file_name_from_hash(key_hash)};
        ::fast_io::io::print(ref, name);
        return path;
    }

    [[nodiscard]] inline constexpr cache_status store_object(cache_context const& ctx,
                                                             ::std::byte const* object,
                                                             ::std::size_t size,
                                                             cache_policy const& policy) noexcept
    {
        if(!policy.enable) { return cache_status::disabled; }

        ::uwvm2::utils::container::vector<::std::byte> blob{};
        if(auto const status{details::build_cache_blob(ctx, object, size, policy, blob)}; status != cache_status::ok) [[unlikely]] { return status; }

        return details::write_cache_blob_atomic(ctx, blob);
    }

    namespace details
    {
        struct cache_store_request
        {
            cache_context ctx{};
            ::uwvm2::utils::container::vector<::std::byte> blob{};
            ::uwvm2::utils::container::u8string log_module{};
            ::std::size_t object_bytes{};
            ::std::size_t object_index{};
            ::std::size_t object_count{};
            bool log_completion{};
            bool log_parallel_object{};
        };

        template <typename... Args>
        inline constexpr void runtime_cache_log_line(Args&&... args) noexcept
        {
#if defined(UWVM)
            if(!::uwvm2::uwvm::io::enable_runtime_log) { return; }

            ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output, u8"[llvm-jit-cache] ", ::std::forward<Args>(args)...);
#else
            ((void)args, ...);
#endif
        }

        inline constexpr void log_cache_store_completion(cache_store_request const& request, cache_status status) noexcept
        {
            if(!request.log_completion) { return; }
            if(request.log_parallel_object)
            {
                runtime_cache_log_line(u8"object-cache-store-complete module=\"",
                                       request.log_module,
                                       u8"\" status=",
                                       cache_status_name(status),
                                       u8" bytes=",
                                       request.object_bytes,
                                       u8" object_index=",
                                       request.object_index,
                                       u8" object_count=",
                                       request.object_count);
                return;
            }

            runtime_cache_log_line(u8"object-cache-store-complete module=\"",
                                   request.log_module,
                                   u8"\" status=",
                                   cache_status_name(status),
                                   u8" bytes=",
                                   request.object_bytes);
        }

        struct async_cache_store_worker
        {
            ::std::mutex mutex{};
            ::std::condition_variable condition{};
            ::std::thread worker{};
            ::std::deque<cache_store_request> requests{};
            ::std::size_t active_requests{};
            bool stop_requested{};
            bool worker_started{};

            async_cache_store_worker() noexcept = default;
            async_cache_store_worker(async_cache_store_worker const&) = delete;
            async_cache_store_worker& operator= (async_cache_store_worker const&) = delete;

            ~async_cache_store_worker() noexcept { this->stop_and_join(); }

            inline constexpr void run() noexcept
            {
                for(;;)
                {
                    cache_store_request request{};

                    {
                        ::std::unique_lock lock{this->mutex};
                        this->condition.wait(lock, [this]() noexcept { return this->stop_requested || !this->requests.empty(); });
                        if(this->requests.empty())
                        {
                            if(this->stop_requested) { break; }
                            continue;
                        }

                        request = ::std::move(this->requests.front());
                        this->requests.pop_front();
                        ++this->active_requests;
                    }

                    auto const status{write_cache_blob_atomic(request.ctx, request.blob)};
                    log_cache_store_completion(request, status);

                    {
                        ::std::lock_guard lock{this->mutex};
                        --this->active_requests;
                    }
                    this->condition.notify_all();
                }
            }

            [[nodiscard]] inline constexpr bool start_locked() noexcept
            {
                if(this->worker_started) { return true; }

#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    this->worker = ::std::thread{[this]() noexcept { this->run(); }};
                    this->worker_started = true;
                    return true;
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(...)
                {
                    return false;
                }
#endif
            }

            [[nodiscard]] inline constexpr cache_status enqueue(cache_store_request&& request) noexcept
            {
                bool run_synchronously{};

#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    ::std::lock_guard lock{this->mutex};
                    if(this->stop_requested) { run_synchronously = true; }
                    else if(!this->start_locked()) { run_synchronously = true; }
                    else
                    {
                        this->requests.push_back(::std::move(request));
                        this->condition.notify_one();
                        return cache_status::ok;
                    }
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(...)
                {
                    run_synchronously = true;
                }
#endif

                if(run_synchronously)
                {
                    auto const status{write_cache_blob_atomic(request.ctx, request.blob)};
                    log_cache_store_completion(request, status);
                    return status;
                }

                return cache_status::io_error;
            }

            inline constexpr void flush() noexcept
            {
                ::std::unique_lock lock{this->mutex};
                this->condition.wait(lock, [this]() noexcept { return this->requests.empty() && this->active_requests == 0uz; });
            }

            inline constexpr void stop_and_join() noexcept
            {
                {
                    ::std::lock_guard lock{this->mutex};
                    this->stop_requested = true;
                }
                this->condition.notify_all();

                if(this->worker.joinable()) { this->worker.join(); }
            }
        };

        [[nodiscard]] inline constexpr async_cache_store_worker& async_cache_store_worker_instance() noexcept
        {
            static async_cache_store_worker worker{};  // [global]
            return worker;
        }
    }  // namespace details

    [[nodiscard]] inline constexpr cache_status store_object_async(cache_context const& ctx,
                                                                   ::std::byte const* object,
                                                                   ::std::size_t size,
                                                                   cache_policy const& policy,
                                                                   ::uwvm2::utils::container::u8string_view log_module = {},
                                                                   bool log_completion = false,
                                                                   ::std::size_t object_index = 0uz,
                                                                   ::std::size_t object_count = 0uz) noexcept
    {
        if(!policy.enable) { return cache_status::disabled; }

        details::cache_store_request request{};
        request.ctx = ctx;
        request.object_bytes = size;
        request.object_index = object_index;
        request.object_count = object_count;
        request.log_completion = log_completion;
        request.log_parallel_object = object_count != 0uz;
        if(log_completion)
        {
            request.log_module.reserve(log_module.size());
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(request.log_module)};
            ::fast_io::io::print(ref, log_module);
        }
        if(auto const status{details::build_cache_blob(ctx, object, size, policy, request.blob)}; status != cache_status::ok) [[unlikely]] { return status; }

        return details::async_cache_store_worker_instance().enqueue(::std::move(request));
    }

    inline constexpr void flush_async_store_objects() noexcept { details::async_cache_store_worker_instance().flush(); }

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
            auto const key_hash{details::cache_key_hash(ctx)};
            auto cache_dir{details::open_cache_object_dir(ctx, key_hash, false)};
            auto const file_name{details::cache_file_name_from_hash(key_hash)};
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
                if(view.header.signature != static_cast<::std::uint_least32_t>(signature_kind::ed25519_identity))
                {
                    result.status = cache_status::unsupported_signature;
                    return result;
                }
                if(!cache_ed25519_identity_signature_available)
                {
                    result.status = cache_status::unsupported_signature;
                    return result;
                }

                auto header_bytes{serialize_fixed_header(view.header)};
                auto const isa_size{static_cast<::std::size_t>(view.header.isa_metadata_size)};
                auto const context_size{static_cast<::std::size_t>(view.header.context_metadata_size)};
                auto const payload_size{static_cast<::std::size_t>(view.header.payload_size)};

                if(!details::ed25519_identity_verify(
                       ctx, header_bytes, view.isa_metadata, isa_size, view.context_metadata, context_size, view.signature, view.payload, payload_size))
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

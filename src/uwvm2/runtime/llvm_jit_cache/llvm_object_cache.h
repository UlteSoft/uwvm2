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
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <string>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/ADT/StringRef.h>
#  include <llvm/Bitcode/BitcodeWriter.h>
#  include <llvm/ExecutionEngine/ObjectCache.h>
#  include <llvm/IR/Module.h>
#  include <llvm/Support/MemoryBuffer.h>
#  include <llvm/Support/raw_ostream.h>
# endif
// import
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include "environment.h"
# include "store.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    namespace details
    {
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string llvm_ref_to_u8(llvm::StringRef ref) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            if(!ref.empty())
            {
                ::uwvm2::utils::container::u8string_ref_uwvm out_ref{::std::addressof(out)};
                ::fast_io::io::print(out_ref, ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(ref.data()), ref.size()});
            }
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string module_bitcode_hash(llvm::Module const& module) UWVM_THROWS
        {
            ::std::string bitcode{};
            ::llvm::raw_string_ostream os{bitcode};
            ::llvm::WriteBitcodeToFile(module, os);
            os.flush();

            auto const first{reinterpret_cast<::std::byte const*>(bitcode.data())};
            ::uwvm2::utils::container::vector<::std::byte> bytes{};
            bytes.reserve(bitcode.size());
            append_bytes(bytes, first, first + bitcode.size());
            return sha256_hex_for_path(bytes);
        }
    }  // namespace details

    class llvm_jit_object_cache final : public ::llvm::ObjectCache
    {
        cache_context base_context{};
        cache_policy policy{};

        [[nodiscard]] inline constexpr cache_context make_module_context(::llvm::Module const& module) const UWVM_THROWS
        {
            auto ctx{base_context};
            ::uwvm2::utils::container::u8string key{};
            ::uwvm2::utils::container::u8string_ref_uwvm key_ref{::std::addressof(key)};
            ::fast_io::io::print(key_ref, ctx.cache_key, u8'\n', details::llvm_ref_to_u8(module.getModuleIdentifier()), u8'\n', details::module_bitcode_hash(module));
            ctx.cache_key = ::std::move(key);
            return ctx;
        }

        inline static constexpr void warn_signature_missing_once() noexcept
        {
# if defined(UWVM)
            static ::std::atomic_bool warned{}; // [global]
            if(warned.exchange(true, ::std::memory_order_relaxed)) { return; }

            if(!::uwvm2::uwvm::io::show_runtime_warning) { return; }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT cache object has no identity signature; rejected because signature verification is enabled. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"Use --runtime-llvm-jit-cache-no-verify only when unsigned cache objects are trusted.",
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
# endif
        }

    public:
        inline constexpr llvm_jit_object_cache() noexcept = default;

        inline explicit constexpr llvm_jit_object_cache(cache_context ctx, cache_policy pol = default_cache_policy()) noexcept :
            base_context{::std::move(ctx)}, policy{pol}
        {
        }

        inline constexpr void notifyObjectCompiled(::llvm::Module const* module, ::llvm::MemoryBufferRef object) UWVM_THROWS override
        {
            if(module == nullptr) [[unlikely]] { return; }
            auto const buffer{object.getBuffer()};
            auto const first{reinterpret_cast<::std::byte const*>(buffer.data())};
            (void)store_object(make_module_context(*module), first, buffer.size(), policy);
        }

        [[nodiscard]] inline constexpr ::std::unique_ptr<::llvm::MemoryBuffer> getObject(::llvm::Module const* module) UWVM_THROWS override
        {
            if(module == nullptr) [[unlikely]] { return {}; }
            auto load{load_object(make_module_context(*module), policy)};
            if(load.status != cache_status::ok)
            {
                if(load.status == cache_status::signature_missing) [[unlikely]] { warn_signature_missing_once(); }
                return {};
            }
            auto const first{reinterpret_cast<char const*>(load.object.data())};
            return ::llvm::MemoryBuffer::getMemBufferCopy(::llvm::StringRef{first, load.object.size()}, "uwvm2-llvm-jit-cache");
        }
    };
}  // namespace uwvm2::runtime::llvm_jit_cache
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

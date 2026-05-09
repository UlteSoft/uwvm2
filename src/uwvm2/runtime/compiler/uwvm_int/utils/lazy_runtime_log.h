/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <utility>
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/thread/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log
{
    [[nodiscard]] inline constexpr ::fast_io::u8string_view compile_state_name(::uwvm2::utils::thread::lazy_compile_state st) noexcept
    {
        switch(st)
        {
            case ::uwvm2::utils::thread::lazy_compile_state::uncompiled: return u8"uncompiled";
            case ::uwvm2::utils::thread::lazy_compile_state::queued: return u8"queued";
            case ::uwvm2::utils::thread::lazy_compile_state::compiling: return u8"compiling";
            case ::uwvm2::utils::thread::lazy_compile_state::compiled: return u8"compiled";
            case ::uwvm2::utils::thread::lazy_compile_state::failed: return u8"failed";
            [[unlikely]] default: return u8"unknown";
        }
    }

    [[nodiscard]] inline bool enabled() noexcept
    {
#ifdef UWVM
        return ::uwvm2::uwvm::io::enable_runtime_log;
#else
        return false;
#endif
    }

    template <typename... Args>
    inline void line(Args&&... args) noexcept
    {
#ifdef UWVM
        if(!enabled()) { return; }

        auto u8runtime_log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8runtime_log_output)};
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8runtime_log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8runtime_log_output_osr)};
        auto u8runtime_log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8runtime_log_output_osr)};

        ::fast_io::io::perrln(u8runtime_log_output_ul, u8"[uwvm-int-lazy] ", ::std::forward<Args>(args)...);
#else
        ((void)args, ...);
#endif
    }
}  // namespace uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log

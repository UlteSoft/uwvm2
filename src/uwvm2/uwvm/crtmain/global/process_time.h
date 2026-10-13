/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-02
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <atomic>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::global
{
    // Process-wide diagnostic timestamps are shared by admitted host entries.
    // Serialize only recording/snapshotting; no generated instruction uses this lock.
    inline ::std::atomic_flag wasm_time_lock = ATOMIC_FLAG_INIT;
    struct wasm_time_guard
    {
        inline constexpr wasm_time_guard() noexcept
        { while(wasm_time_lock.test_and_set(::std::memory_order_acquire)) {} }
        inline constexpr ~wasm_time_guard() { wasm_time_lock.clear(::std::memory_order_release); }
        wasm_time_guard(wasm_time_guard const&) = delete;
        wasm_time_guard& operator=(wasm_time_guard const&) = delete;
    };
    inline ::fast_io::unix_timestamp wasm_start_time{};
    inline ::fast_io::unix_timestamp wasm_end_time{};
    inline bool wasm_start_time_available{};
    inline bool wasm_end_time_available{};

    [[nodiscard]] UWVM_GNU_COLD inline constexpr bool try_get_monotonic_raw_time(::fast_io::unix_timestamp & timestamp) noexcept
    {
#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            timestamp = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            return true;
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            return false;
        }
#endif
    }

    UWVM_GNU_COLD inline constexpr void record_total_wasm_time_start() noexcept
    {
        wasm_time_guard guard{};
        if(wasm_start_time_available) [[unlikely]] { return; }
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"Begin running the WASM program. ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                u8"[",
                                ::uwvm2::uwvm::io::get_local_realtime(),
                                u8"] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        }
        wasm_start_time_available = try_get_monotonic_raw_time(wasm_start_time);
        wasm_end_time_available = false;
    }

    UWVM_GNU_COLD inline constexpr void record_total_wasm_time_end() noexcept
    {
        wasm_time_guard guard{};
        if(!wasm_start_time_available) [[unlikely]] { return; }
        wasm_end_time_available = try_get_monotonic_raw_time(wasm_end_time);
    }

    UWVM_GNU_COLD inline constexpr void discard_total_wasm_time_record() noexcept
    {
        wasm_time_guard guard{};
        wasm_start_time_available = false;
        wasm_end_time_available = false;
    }

    template <typename Label>
    UWVM_GNU_COLD inline constexpr void print_verbose_total_time(Label && label, ::fast_io::unix_timestamp duration) noexcept
    {
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            label,
                            u8": ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            duration,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"s. ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                            u8"[",
                            ::uwvm2::uwvm::io::get_local_realtime(),
                            u8"] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                            u8"(verbose)\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
    }

    /// @brief      Record the process runtime and emit it in verbose logging at scope exit.
    struct process_time
    {
        ::fast_io::unix_timestamp start_time{};
        bool start_time_available{};

        UWVM_GNU_COLD inline constexpr process_time() noexcept { start_time_available = try_get_monotonic_raw_time(start_time); }

        inline constexpr process_time(process_time const&) noexcept = delete;
        inline constexpr process_time(process_time&&) noexcept = delete;
        inline constexpr process_time& operator= (process_time const&) noexcept = delete;
        inline constexpr process_time& operator= (process_time&&) noexcept = delete;

        UWVM_GNU_COLD inline constexpr ~process_time()
        {
            if(!start_time_available) [[unlikely]] { return; }
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::fast_io::unix_timestamp end_time{};

                if(!try_get_monotonic_raw_time(end_time)) [[unlikely]] { return; }

                ::fast_io::unix_timestamp duration{};
                bool available{};
                {
                    wasm_time_guard guard{};
                    available = wasm_start_time_available;
                    if(available) { duration = (wasm_end_time_available ? wasm_end_time : end_time) - wasm_start_time; }
                }
                if(available) { print_verbose_total_time(u8"Total WASM execution time", duration); }

                print_verbose_total_time(u8"Total process time", end_time - start_time);
            }
        }
    };
}  // namespace uwvm2::uwvm::global

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

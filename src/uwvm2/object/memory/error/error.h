/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <new>
# include <atomic>
# include <bit>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::error
{

    /// @brief      Basic memory 65-bit offset
    /// @note       In wasm64, the maximum memory size can be obtained by summing (u64)static_offset and (u64)dynamic_offset, resulting in a maximum memory size
    ///             of u65.
    struct basic_memory_65bit_offset_t
    {
        ::std::uint_least64_t offset;
        bool offset_65_bit;
    };

    inline constexpr ::std::uint_least64_t get_dynamic_offset(basic_memory_65bit_offset_t memory_offset, ::std::uint_least64_t memory_static_offset) noexcept
    {
        // Regardless of whether 65-bit architecture is present, the total address is derived from static_offset + dynamic_offset. If 65-bit architecture is
        // present, offset_low64 < static_offset. The correct dynamic_offset value can be obtained by performing an unsigned wrap-around calculation:
        // offset_low64 - static_offset. If 65-bit architecture is not present, the correct result can be obtained through normal subtraction.
        return memory_offset.offset - memory_static_offset;
    }

    struct memory_error_t
    {
        ::std::size_t memory_idx{};
        basic_memory_65bit_offset_t memory_offset{};
        ::std::uint_least64_t memory_static_offset{};
        ::std::uint_least64_t memory_length{};
        ::std::size_t memory_type_size{};
    };

    inline constexpr void output_memory_error(memory_error_t const& memerr) noexcept
    {
#ifdef UWVM
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"memory[",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            memerr.memory_idx,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"] access overflow: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            ::fast_io::mnp::addrvw(memerr.memory_static_offset),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" (static) + ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            ::fast_io::mnp::addrvw(get_dynamic_offset(memerr.memory_offset, memerr.memory_static_offset)),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" (dynamic) + ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            memerr.memory_type_size,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" (bytes) > ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::addrvw(memerr.memory_length),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" (allocated)\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#else
        ::fast_io::io::perr(::fast_io::u8err(),
                            u8"uwvm: "
                            u8"[fatal] "
                            u8"memory[",
                            memerr.memory_idx,
                            u8"] access overflow: ",
                            ::fast_io::mnp::addrvw(memerr.memory_static_offset),
                            u8" (static) + ",
                            ::fast_io::mnp::addrvw(get_dynamic_offset(memerr.memory_offset, memerr.memory_static_offset)),
                            u8" (dynamic) + ",
                            memerr.memory_type_size,
                            u8" (bytes) > ",
                            ::fast_io::mnp::addrvw(memerr.memory_length),
                            u8" (allocated)\n\n");
#endif
    }

    [[noreturn]] inline void output_memory_error_and_terminate(memory_error_t const& memerr) noexcept
    {
        output_memory_error(memerr);
        ::fast_io::fast_terminate();
    }

}  // namespace uwvm2::object::memory::error

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

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
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include "wasm32.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::abi
{
    /// @brief    WASI ABI for WASM64
    /// @details  In the traditional wasip1, the ABI is based on 32-bit pointers (i32 offset into linear memory). When memory64 support was introduced, to
    ///           maintain compatibility with preview1 programs, a set of mirrored APIs was provided, simply appending `_wasm64` to their names. Although some
    ///           programs remain wasm64-wasip1 (32-bit ABI), this does not affect usage. Only portions of pointers exceeding 4GB are truncated. Thus, providing
    ///           a memory64 compatibility mode for wasip1 ensures full compatibility.

    // static assert

    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i8) == 1uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8) == 1uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i16) == 2uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16) == 2uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32) == 4uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32) == 4uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64) == 8uz, "non-wasi data layout");
    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64) == 8uz, "non-wasi data layout");

    // wasm64 pointer-sized types
    using wasi_intptr_wasm64_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasi_uintptr_wasm64_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
    using wasi_size_wasm64_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
    using wasi_void_ptr_wasm64_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;

    using wasi_posix_fd_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;

    using advice_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::advice_t;

    using clockid_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::clockid_t;

    using device_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::device_t;

    using dirnamlen_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::dirnamlen_t;

    using dircookie_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::dircookie_t;

    using errno_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t;

    using eventrwflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t;

    using eventtype_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::eventtype_t;

    using exitcode_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::exitcode_t;

    using fd_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::fd_t;

    using fdflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::fdflags_t;

    using filedelta_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::filedelta_t;

    using filesize_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::filesize_t;

    using filetype_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::filetype_t;

    using fstflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::fstflags_t;

    using inode_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::inode_t;

    using linkcount_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::linkcount_t;

    using lookupflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t;

    using oflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::oflags_t;

    using riflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::riflags_t;

    using rights_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::rights_t;

    using roflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::roflags_t;

    using sdflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::sdflags_t;

    using siflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::siflags_t;

    using signal_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::signal_t;

    using subclockflags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t;

    using timestamp_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::timestamp_t;

    using userdata_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::userdata_t;

    using whence_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::whence_t;

    using preopentype_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::preopentype_t;

    struct alignas(8uz) wasi_iovec_wasm64_t
    {
        wasi_void_ptr_wasm64_t buf;
        wasi_size_wasm64_t buf_len;
    };

    inline constexpr ::std::size_t size_of_wasi_iovec_wasm64_t{16uz};

    inline consteval bool is_default_wasi_iovec_wasm64_data_layout() noexcept
    {
        // In standard layout mode, data can be transferred in a single memcpy operation (static length), improving read efficiency.
        return __builtin_offsetof(wasi_iovec_wasm64_t, buf) == 0uz && __builtin_offsetof(wasi_iovec_wasm64_t, buf_len) == 8uz &&
               sizeof(wasi_iovec_wasm64_t) == size_of_wasi_iovec_wasm64_t && alignof(wasi_iovec_wasm64_t) == 8uz &&
               ::std::endian::native == ::std::endian::little;
    }

    using wasi_ciovec_wasm64_t = wasi_iovec_wasm64_t;

    inline constexpr ::std::size_t size_of_wasi_ciovec_wasm64_t{size_of_wasi_iovec_wasm64_t};

    inline consteval bool is_default_wasi_ciovec_wasm64_data_layout() noexcept { return is_default_wasi_iovec_wasm64_data_layout(); }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

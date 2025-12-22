/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-24
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

module;

/// @note   To maintain compatibility with fast_io, the fast_io style of writing is adopted here.
#include "impl.h"

export module uwvm2.utils.allocator.fast_io_strict;

export namespace uwvm2::utils::allocator::fast_io_strict
{
    using ::uwvm2::utils::allocator::fast_io_strict::fast_io_allocator_to_strict;

    using ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter;
    using ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_typed_generic_allocator_adapter;

    using ::uwvm2::utils::allocator::fast_io_strict::native_strict_global_allocator;
    using ::uwvm2::utils::allocator::fast_io_strict::native_strict_thread_local_allocator;
    using ::uwvm2::utils::allocator::fast_io_strict::native_strict_typed_global_allocator;
    using ::uwvm2::utils::allocator::fast_io_strict::native_strict_typed_thread_local_allocator;
}  // namespace uwvm2::utils::allocator::fast_io_strict

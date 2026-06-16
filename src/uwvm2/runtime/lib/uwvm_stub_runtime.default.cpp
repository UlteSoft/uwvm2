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

#ifndef UWVM_MODULE
// import
# include <fast_io.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
#endif

namespace uwvm2::runtime::lib
{
    extern "C++" void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config cfg) noexcept
    {
        static_cast<void>(main_module_name);
        static_cast<void>(cfg);
        ::fast_io::fast_terminate();
    }

    extern "C++" void lazy_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, lazy_compile_run_config cfg) noexcept
    {
        static_cast<void>(main_module_name);
        static_cast<void>(cfg);
        ::fast_io::fast_terminate();
    }

    extern "C++" void lazy_compile_stop_before_proc_exit_host_api() noexcept {}

    extern "C++" ::std::size_t preload_memory_descriptor_count_host_api() noexcept { return 0uz; }

    extern "C++" bool preload_memory_descriptor_at_host_api(::std::size_t descriptor_index,
                                                            ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t* out) noexcept
    {
        static_cast<void>(descriptor_index);
        static_cast<void>(out);
        return false;
    }

    extern "C++" bool preload_memory_read_host_api(::std::size_t memory_index,
                                                   ::std::uint_least64_t offset,
                                                   void* destination,
                                                   ::std::size_t size) noexcept
    {
        static_cast<void>(memory_index);
        static_cast<void>(offset);
        static_cast<void>(destination);
        static_cast<void>(size);
        return false;
    }

    extern "C++" bool preload_memory_write_host_api(::std::size_t memory_index,
                                                    ::std::uint_least64_t offset,
                                                    void const* source,
                                                    ::std::size_t size) noexcept
    {
        static_cast<void>(memory_index);
        static_cast<void>(offset);
        static_cast<void>(source);
        static_cast<void>(size);
        return false;
    }
}  // namespace uwvm2::runtime::lib

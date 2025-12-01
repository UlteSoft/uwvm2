/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_readdir.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(2uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table
    env.fd_storage.opens.resize(10uz);

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir(env,
                                                                           static_cast<wasi_posix_fd_t>(-1),
                                                                           static_cast<wasi_void_ptr_t>(0u),
                                                                           static_cast<wasi_size_t>(0u),
                                                                           static_cast<::uwvm2::imported::wasi::wasip1::abi::dircookie_t>(0u),
                                                                           static_cast<wasi_void_ptr_t>(0u));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: expected ebadf for negative fd: ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: directory enumeration writes entries and reports buf_used
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);

        // initialize as directory in new fd model
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        auto& dir_stack = fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
            dir_stack.dir_stack.push_back(::std::move(entry));
        }

        constexpr wasi_void_ptr_t buf_ptr{4096u};
        constexpr wasi_void_ptr_t used_ptr{2048u};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir(env,
                                                                           static_cast<wasi_posix_fd_t>(4),
                                                                           buf_ptr,
                                                                           static_cast<wasi_size_t>(1024u),
                                                                           static_cast<::uwvm2::imported::wasi::wasip1::abi::dircookie_t>(0u),
                                                                           used_ptr);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case1 expected esuccess: ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const used = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, used_ptr);
        if(used > static_cast<wasi_size_t>(1024u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case1 used exceeds buf limit: ", static_cast<unsigned>(used));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: rights check enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        // rights default 0

        // initialize as directory in new fd model
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        auto& dir_stack = fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
            dir_stack.dir_stack.push_back(::std::move(entry));
        }

        constexpr wasi_void_ptr_t buf_ptr{12288u};
        constexpr wasi_void_ptr_t used_ptr{12000u};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir(env,
                                                                           static_cast<wasi_posix_fd_t>(5),
                                                                           buf_ptr,
                                                                           static_cast<wasi_size_t>(128u),
                                                                           static_cast<::uwvm2::imported::wasi::wasip1::abi::dircookie_t>(0u),
                                                                           used_ptr);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: rights check expected enotcapable: ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: preload ".." inode must be 0; descend into 'a', then back to preload, '..' becomes 0 again
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);

        // preload root '.'
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        auto& dir_stack = fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        dir_stack.dir_stack.clear();
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
            dir_stack.dir_stack.push_back(::std::move(entry));
        }

        constexpr wasi_void_ptr_t buf_ptr0{13056u};
        constexpr wasi_void_ptr_t used_ptr0{13312u};

        // read at preload
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir(env,
                                                                               static_cast<wasi_posix_fd_t>(6),
                                                                               buf_ptr0,
                                                                               static_cast<wasi_size_t>(512u),
                                                                               static_cast<::uwvm2::imported::wasi::wasip1::abi::dircookie_t>(0u),
                                                                               used_ptr0);
            if(ret != errno_t::esuccess)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 preload read expected esuccess: ", static_cast<unsigned>(ret));
                ::fast_io::fast_terminate();
            }
            auto const used0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, used_ptr0);
            // parse second entry ('..') d_ino (offset header + name len)
            auto const second_header_off = static_cast<::std::size_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_dirent_t + 1u);
            if(used0 < second_header_off + ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_dirent_t)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 preload used0 too small: ", static_cast<unsigned>(used0));
                ::fast_io::fast_terminate();
            }
            auto const dino_lo =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, buf_ptr0 + second_header_off + 8u);
            auto const dino_hi =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, buf_ptr0 + second_header_off + 12u);
            auto const dino = (static_cast<unsigned long long>(dino_hi) << 32) | static_cast<unsigned long long>(dino_lo);
            if(dino != 0ull)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 '..' inode at preload must be 0, got ", dino);
                ::fast_io::fast_terminate();
            }
        }

        // ensure a/ exists, then descend: stack = [root, a]
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"a");
        }
        catch(...)
        {
            return 0;
        }

        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"a"};
            dir_stack.dir_stack.push_back(::std::move(entry));
        }

        // read at 'a' (parent inode should be non-zero and equals root ino)
        constexpr wasi_void_ptr_t buf_ptr1{13568u};
        constexpr wasi_void_ptr_t used_ptr1{13824u};
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir(env,
                                                                               static_cast<wasi_posix_fd_t>(6),
                                                                               buf_ptr1,
                                                                               static_cast<wasi_size_t>(512u),
                                                                               static_cast<::uwvm2::imported::wasi::wasip1::abi::dircookie_t>(0u),
                                                                               used_ptr1);
            if(ret != errno_t::esuccess)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 read at 'a' expected esuccess: ", static_cast<unsigned>(ret));
                ::fast_io::fast_terminate();
            }
            auto const used1 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, used_ptr1);
            auto const second_header_off = static_cast<::std::size_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_dirent_t + 1u);
            if(used1 < second_header_off + ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_dirent_t)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 used1 too small: ", static_cast<unsigned>(used1));
                ::fast_io::fast_terminate();
            }
            auto const dino_lo =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, buf_ptr1 + second_header_off + 8u);
            auto const dino_hi =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, buf_ptr1 + second_header_off + 12u);
            [[maybe_unused]] auto const dino = (static_cast<unsigned long long>(dino_hi) << 32) | static_cast<unsigned long long>(dino_lo);

            // compute root ino
            ::fast_io::posix_file_status st_root{status(dir_stack.dir_stack.front_unchecked().ptr->dir_stack.storage.file)};
            [[maybe_unused]] auto const root_ino = static_cast<unsigned long long>(st_root.ino);

#if !(defined(_WIN32) && defined(_WIN32_WINDOWS)) // Win9x uses pathname emulation, so you can tell directly.
            if(dino == 0ull || dino != root_ino)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 parent inode mismatch: dino=", dino, u8", root_ino=", root_ino);
                ::fast_io::fast_terminate();
            }
#endif
        }

        // go back to preload (pop 'a'), '..' must be 0 again
        dir_stack.dir_stack.resize(1uz);
        constexpr wasi_void_ptr_t buf_ptr2{14080u};
        constexpr wasi_void_ptr_t used_ptr2{14336u};
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir(env,
                                                                               static_cast<wasi_posix_fd_t>(6),
                                                                               buf_ptr2,
                                                                               static_cast<wasi_size_t>(512u),
                                                                               static_cast<::uwvm2::imported::wasi::wasip1::abi::dircookie_t>(0u),
                                                                               used_ptr2);
            if(ret != errno_t::esuccess)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 return to preload expected esuccess: ", static_cast<unsigned>(ret));
                ::fast_io::fast_terminate();
            }
            auto const used2 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, used_ptr2);
            auto const second_header_off = static_cast<::std::size_t>(::uwvm2::imported::wasi::wasip1::func::size_of_wasi_dirent_t + 1u);
            if(used2 < second_header_off + ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_dirent_t)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 used2 too small: ", static_cast<unsigned>(used2));
                ::fast_io::fast_terminate();
            }
            auto const dino_lo =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, buf_ptr2 + second_header_off + 8u);
            auto const dino_hi =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, buf_ptr2 + second_header_off + 12u);
            auto const dino = (static_cast<unsigned long long>(dino_hi) << 32) | static_cast<unsigned long long>(dino_lo);
            if(dino != 0ull)
            {
                ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_readdir: case3 '..' inode after back to preload must be 0, got ", dino);
                ::fast_io::fast_terminate();
            }
        }

        // cleanup a/
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"a", ::fast_io::native_at_flags::removedir);
        }
        catch(...)
        {
        }
    }

    return 0;
}


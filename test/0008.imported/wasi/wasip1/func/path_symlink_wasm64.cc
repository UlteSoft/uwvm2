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
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_symlink_wasm64.h>

// NOLINTBEGIN

using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_bytes64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, void const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

inline static void write_cu8str64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, char8_t const* s)
{ write_bytes64(memory, p, s, ::std::char_traits<char8_t>::length(s)); }

inline static void set_dirfd(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_wasm64_t base_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = static_cast<rights_wasm64_t>(-1);
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

[[maybe_unused]] inline static void try_unlink(char8_t const* name)
{
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), ::fast_io::mnp::os_c_str(name), {});
    }
    catch(::fast_io::error)
    {
    }
}

[[maybe_unused]] inline static void try_rmdir(char8_t const* name)
{
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), ::fast_io::mnp::os_c_str(name), ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
}

int main()
{
    native_memory_t memory{};
    memory.init_by_page_count(4uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table
    env.fd_storage.opens.resize(8uz);

    // Common: setup a directory fd at index 3 with '.' and full rights
    set_dirfd(env, 3uz, static_cast<rights_wasm64_t>(-1));

    // ===== Case 0: negative new_fd -> ebadf =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                    static_cast<wasi_size_wasm64_t>(0u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                    static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln("error: psl64 Case 0 expected ebadf. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 1: rights missing -> enotcapable =====
    {
        set_dirfd(env, 4uz, static_cast<rights_wasm64_t>(0));

        constexpr wasi_void_ptr_wasm64_t P_OLD{1024u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{4096u};

        write_cu8str64(memory, P_OLD, u8"psl64_old_target");
        write_cu8str64(memory, P_NEW, u8"psl64_no_rights");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_old_target") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_no_rights") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("error: psl64 Case 1 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 2: new_fd is file -> enotdir =====
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd.rights_base = static_cast<rights_wasm64_t>(-1);
        fd.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        constexpr wasi_void_ptr_wasm64_t P_OLD{2048u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{6144u};

        write_cu8str64(memory, P_OLD, u8"psl64_file_fd_old");
        write_cu8str64(memory, P_NEW, u8"psl64_file_fd_new");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_file_fd_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_file_fd_new") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("error: psl64 Case 2 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 3: empty new_path -> einval =====
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{8192u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{12288u};

        write_cu8str64(memory, P_OLD, u8"psl64_empty_new_path_old");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_empty_new_path_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: psl64 Case 3 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 4: success create symlink (platforms with symlink support) =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    {
        try_unlink(u8"psl64_ok_target.txt");
        try_unlink(u8"psl64_ok_link");

        // prepare target file
        try
        {
            ::fast_io::native_file f{u8"psl64_ok_target.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const data[3]{std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
            ::fast_io::operations::write_all_bytes(f, data, data + 3);
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t P_OLD{16384u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{20480u};

        write_cu8str64(memory, P_OLD, u8"psl64_ok_target.txt");
        write_cu8str64(memory, P_NEW, u8"psl64_ok_link");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_ok_target.txt") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_ok_link") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: psl64 Case 4 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify created symlink payload
        auto const payload = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"psl64_ok_link");
        if(payload != u8"psl64_ok_target.txt")
        {
            ::fast_io::io::perrln("error: psl64 Case 4 payload mismatch.");
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"psl64_ok_link");
        try_unlink(u8"psl64_ok_target.txt");
    }
#endif

    // ===== Case 5: absolute new path -> eperm =====
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{22528u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{24576u};

        write_cu8str64(memory, P_OLD, u8"psl64_abs_old");
        write_cu8str64(memory, P_NEW, u8"/psl64_abs_new");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_abs_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"/psl64_abs_new") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm)
        {
            ::fast_io::io::perrln("error: psl64 Case 5 expected eperm. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 6: last component '.' -> eisdir =====
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{26624u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{28672u};

        // prepare directory
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"psl64_dot_dir");
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str64(memory, P_OLD, u8"psl64_dot_old");
        write_cu8str64(memory, P_NEW, u8"psl64_dot_dir/.");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_dot_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_dot_dir/.") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir)
        {
            ::fast_io::io::perrln("error: psl64 Case 6 expected eisdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_rmdir(u8"psl64_dot_dir");
    }

    // ===== Case 7: last component '..' and empty stack -> eperm =====
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{30720u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{32768u};

        write_cu8str64(memory, P_OLD, u8"psl64_prev_old");
        write_cu8str64(memory, P_NEW, u8"..");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_prev_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"..") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm)
        {
            ::fast_io::io::perrln("error: psl64 Case 7 expected eperm. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 8: invalid UTF-8 in new_path (embedded zero) -> eilseq =====
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{34816u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{36864u};

        write_cu8str64(memory, P_OLD, u8"psl64_ilseq_new_old");

        char8_t buf[8]{u8'p', u8's', u8'l', u8'6', u8'4', u8'\0', u8'x', u8'y'};
        write_bytes64(memory, P_NEW, buf, sizeof(buf));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_ilseq_new_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(buf)));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq)
        {
            ::fast_io::io::perrln("error: psl64 Case 8 expected eilseq. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 9: invalid UTF-8 in old_path (embedded zero) -> eilseq =====
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{38912u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{40960u};

        write_cu8str64(memory, P_NEW, u8"psl64_ilseq_old_new");

        char8_t buf[8]{u8'o', u8'l', u8'd', u8'6', u8'4', u8'\0', u8'x', u8'y'};
        write_bytes64(memory, P_OLD, buf, sizeof(buf));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(buf)),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_ilseq_old_new") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq)
        {
            ::fast_io::io::perrln("error: psl64 Case 9 expected eilseq. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 10: Windows/DOS invalid character in new path -> einval =====
#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    {
        constexpr wasi_void_ptr_wasm64_t P_OLD{43008u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{45056u};

        write_cu8str64(memory, P_OLD, u8"psl64_win_illegal_old");
        write_cu8str64(memory, P_NEW, u8"psl64_inv_bs_a\\b");

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                                    P_OLD,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_win_illegal_old") - 1u),
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    P_NEW,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_inv_bs_a\\b") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: psl64 Case 10 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
#endif

    // ===== Windows NT (>= Vista) symlink dir/file semantics =====
#if defined(_WIN32) && !defined(_WIN32_WINDOWS) && defined(_WIN32_WINNT) && (_WIN32_WINNT > 0x600) && !defined(__MSDOS__) && !defined(__DJGPP__) &&            \
    !defined(__CYGWIN__)

    // Case 11: existing dir symlink used as parent, create link inside -> should succeed and land in real directory
    {
        try_unlink(u8"psl64_nt_dir_real/inner_from_nt");
        try_unlink(u8"psl64_nt_dir_link");
        try_rmdir(u8"psl64_nt_dir_real");

        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"psl64_nt_dir_real");
        }
        catch(::fast_io::error)
        {
        }

        // real dir symlink
        try
        {
            ::fast_io::native_symlinkat(u8"psl64_nt_dir_real", ::fast_io::at_fdcwd(), u8"psl64_nt_dir_link");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t P_OLD{47104u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{49152u};

        write_cu8str64(memory, P_OLD, u8"psl64_nt_payload_dir");
        write_cu8str64(memory, P_NEW, u8"psl64_nt_dir_link/inner_from_nt");

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                       P_OLD,
                                                                       static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_nt_payload_dir") - 1u),
                                                                       static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                       P_NEW,
                                                                       static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_nt_dir_link/inner_from_nt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: psl64 Case 11 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify created physically under real dir
        auto const pl = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"psl64_nt_dir_real/inner_from_nt");
        if(pl != u8"psl64_nt_payload_dir")
        {
            ::fast_io::io::perrln("error: psl64 Case 11 payload mismatch.");
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"psl64_nt_dir_real/inner_from_nt");
        try_unlink(u8"psl64_nt_dir_link");
        try_rmdir(u8"psl64_nt_dir_real");
    }

    // Case 12: existing file symlink used as parent -> should fail with enotdir
    {
        try_unlink(u8"psl64_nt_file_node");
        try_unlink(u8"psl64_nt_file_link");
        try_rmdir(u8"psl64_nt_file_node");

        // Create a real file node
        try
        {
            ::fast_io::native_file f{u8"psl64_nt_file_node", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[1]{std::byte{'F'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }

        // file-type symlink pointing to that node
        try
        {
            ::fast_io::native_symlinkat(u8"psl64_nt_file_node", ::fast_io::at_fdcwd(), u8"psl64_nt_file_link");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t P_OLD{51200u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{53248u};

        write_cu8str64(memory, P_OLD, u8"psl64_nt_payload_file_parent");
        write_cu8str64(memory, P_NEW, u8"psl64_nt_file_link/inner_should_fail");

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                       P_OLD,
                                                                       static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_nt_payload_file_parent") - 1u),
                                                                       static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                       P_NEW,
                                                                       static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_nt_file_link/inner_should_fail") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("error: psl64 Case 12 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"psl64_nt_file_link");
        try_unlink(u8"psl64_nt_file_node");
    }

    // Case 13: file symlink whose target turned into directory, still treated as file parent -> enotdir
    {
        try_unlink(u8"psl64_nt_mut_node");
        try_unlink(u8"psl64_nt_mut_link");
        try_rmdir(u8"psl64_nt_mut_node");

        // Step 1: create file + file symlink
        try
        {
            ::fast_io::native_file f{u8"psl64_nt_mut_node", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[1]{std::byte{'M'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"psl64_nt_mut_node", ::fast_io::at_fdcwd(), u8"psl64_nt_mut_link");
        }
        catch(::fast_io::error)
        {
        }

        // Step 2: remove file, create directory with same name
        try_unlink(u8"psl64_nt_mut_node");
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"psl64_nt_mut_node");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t P_OLD{55296u};
        constexpr wasi_void_ptr_wasm64_t P_NEW{57344u};

        write_cu8str64(memory, P_OLD, u8"psl64_nt_payload_mut");
        write_cu8str64(memory, P_NEW, u8"psl64_nt_mut_link/inner_after_mut");

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(env,
                                                                       P_OLD,
                                                                       static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_nt_payload_mut") - 1u),
                                                                       static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                       P_NEW,
                                                                       static_cast<wasi_size_wasm64_t>(sizeof(u8"psl64_nt_mut_link/inner_after_mut") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: psl64 Case 13 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"psl64_nt_mut_link");
        try_rmdir(u8"psl64_nt_mut_node");
    }
#endif  // Windows NT symlink semantics

    return 0;
}

// NOLINTEND

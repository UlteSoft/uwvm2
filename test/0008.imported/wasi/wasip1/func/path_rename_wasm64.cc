// path_rename wasm64 tests

#include <cstddef>
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_rename_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
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
{
    write_bytes64(memory, p, s, ::std::char_traits<char8_t>::length(s));
}

inline static void set_dirfd(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_t base_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = static_cast<rights_t>(-1);
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

inline static void try_unlink(char8_t const* name)
{
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), ::fast_io::mnp::os_c_str(name), {});
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

    env.fd_storage.opens.resize(16uz);

    constexpr wasi_void_ptr_wasm64_t P0{1024u};
    constexpr wasi_void_ptr_wasm64_t P1{4096u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                   static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                   static_cast<wasi_size_wasm64_t>(0u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                   static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln("pr64 Case0", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Common dirfds at 3 (old) and 4 (new) with full rights
    set_dirfd(env, 3uz, static_cast<rights_t>(-1));
    set_dirfd(env, 4uz, static_cast<rights_t>(-1));

    // Case 1: missing right_path_rename_source on old -> enotcapable
    {
        set_dirfd(env, 5uz, static_cast<rights_t>(0));
        write_cu8str64(memory, P0, u8"a.txt");
        write_cu8str64(memory, P1, u8"b.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"a.txt") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"b.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("pr64 Case1", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2a: old fd is file -> enotdir
    {
        auto& f = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        f.rights_base = static_cast<rights_t>(-1);
        f.rights_inherit = static_cast<rights_t>(-1);
        f.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str64(memory, P0, u8"x");
        write_cu8str64(memory, P1, u8"y");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(6),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("pr64 Case2a", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2b: new fd is file -> enotdir
    {
        auto& f = *env.fd_storage.opens.index_unchecked(7uz).fd_p;
        f.rights_base = static_cast<rights_t>(-1);
        f.rights_inherit = static_cast<rights_t>(-1);
        f.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str64(memory, P0, u8"x");
        write_cu8str64(memory, P1, u8"y");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(7),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("pr64 Case2b", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: empty old_path -> einval
    {
        write_cu8str64(memory, P1, u8"dst");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(0u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(3u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pr64 Case3", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: absolute path -> eperm
    {
        write_cu8str64(memory, P0, u8"/abs_old");
        write_cu8str64(memory, P1, u8"/abs_new");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"/abs_old") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"/abs_new") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm)
        {
            ::fast_io::io::perrln("pr64 Case4", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: successful rename within same directory
    {
        try_unlink(u8"uwvm_ut_pr64_dst.txt");
        try_unlink(u8"uwvm_ut_pr64_src.txt");
        try
        {
            ::fast_io::native_file f{u8"uwvm_ut_pr64_src.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[1]{std::byte{'Z'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str64(memory, P0, u8"uwvm_ut_pr64_src.txt");
        write_cu8str64(memory, P1, u8"uwvm_ut_pr64_dst.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_pr64_src.txt") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_pr64_dst.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pr64 Case5", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_file f{u8"uwvm_ut_pr64_dst.txt", ::fast_io::open_mode::in};
            ::std::byte b{};
            auto p2 = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
            if(p2 != &b + 1 || b != ::std::byte{'Z'})
            {
                ::fast_io::io::perrln("pr64 verify", static_cast<unsigned>(ret));
                ::fast_io::fast_terminate();
            }
        }
        catch(::fast_io::error)
        {
            ::fast_io::io::perrln("pr64 open dst failed");
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_pr64_dst.txt");
        try_unlink(u8"uwvm_ut_pr64_src.txt");
    }

    {
        set_dirfd(env, 8uz, static_cast<rights_t>(0));
        write_cu8str64(memory, P0, u8"pr64_rn_src1.txt");
        write_cu8str64(memory, P1, u8"pr64_rn_dst1.txt");
        auto const r6 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                  P0,
                                                                                  static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_src1.txt") - 1u),
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(8),
                                                                                  P1,
                                                                                  static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_dst1.txt") - 1u));
        if(r6 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("pr64 Case6", static_cast<unsigned>(r6));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8"pr64_rn_src2.txt");
        auto const r7 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                  P0,
                                                                                  static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_src2.txt") - 1u),
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                  P1,
                                                                                  static_cast<wasi_size_wasm64_t>(0u));
        if(r7 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pr64 Case7", static_cast<unsigned>(r7));
            ::fast_io::fast_terminate();
        }
    }

    {
        unsigned char const bad_utf8[2]{0xC3u, 0x28u};
        write_bytes64(memory, P0, bad_utf8, 2);
        write_cu8str64(memory, P1, u8"valid_new");
        auto const r8 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                  P0,
                                                                                  static_cast<wasi_size_wasm64_t>(2u),
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                  P1,
                                                                                  static_cast<wasi_size_wasm64_t>(sizeof(u8"valid_new") - 1u));
        if(r8 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq)
        {
            ::fast_io::io::perrln("pr64 Case8", static_cast<unsigned>(r8));
            ::fast_io::fast_terminate();
        }
    }

    {
        unsigned char const bad_utf8[2]{0xC3u, 0x28u};
        write_cu8str64(memory, P0, u8"valid_old");
        write_bytes64(memory, P1, bad_utf8, 2);
        auto const r9 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                  P0,
                                                                                  static_cast<wasi_size_wasm64_t>(sizeof(u8"valid_old") - 1u),
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                  P1,
                                                                                  static_cast<wasi_size_wasm64_t>(2u));
        if(r9 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq)
        {
            ::fast_io::io::perrln("pr64 Case9", static_cast<unsigned>(r9));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8".");
        write_cu8str64(memory, P1, u8"pr64_rn_ndot");
        auto const r10 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_ndot") - 1u));
        if(r10 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pr64 Case10", static_cast<unsigned>(r10));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8"..");
        write_cu8str64(memory, P1, u8"pr64_rn_ndotdot");
        auto const r11 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(2u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_ndotdot") - 1u));
        if(r11 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pr64 Case11", static_cast<unsigned>(r11));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8"pr64_rn_old_a");
        write_cu8str64(memory, P1, u8".");
        auto const r12 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_old_a") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(1u));
        if(r12 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pr64 Case12", static_cast<unsigned>(r12));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8"pr64_rn_old_b");
        write_cu8str64(memory, P1, u8"..");
        auto const r13 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_old_b") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(2u));
        if(r13 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pr64 Case13", static_cast<unsigned>(r13));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8"pr64_rn_old_c");
        write_cu8str64(memory, P1, u8"../x");
        auto const r14 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_old_c") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"../x") - 1u));
        if(r14 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm)
        {
            ::fast_io::io::perrln("pr64 Case14", static_cast<unsigned>(r14));
            ::fast_io::fast_terminate();
        }
    }

    {
        write_cu8str64(memory, P0, u8"../x");
        write_cu8str64(memory, P1, u8"pr64_rn_new_d");
        auto const r15 = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"../x") - 1u),
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                   P1,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"pr64_rn_new_d") - 1u));
        if(r15 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm)
        {
            ::fast_io::io::perrln("pr64 Case15", static_cast<unsigned>(r15));
            ::fast_io::fast_terminate();
        }
    }

    {
        try_unlink(u8"uwvm_ut_pr64_missing_src.txt");
        try_unlink(u8"uwvm_ut_pr64_missing_dst.txt");
        write_cu8str64(memory, P0, u8"uwvm_ut_pr64_missing_src.txt");
        write_cu8str64(memory, P1, u8"uwvm_ut_pr64_missing_dst.txt");
        auto const r16 =
            ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                      static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                      P0,
                                                                      static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_pr64_missing_src.txt") - 1u),
                                                                      static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                      P1,
                                                                      static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_pr64_missing_dst.txt") - 1u));
        if(r16 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent)
        {
            ::fast_io::io::perrln("pr64 Case16", static_cast<unsigned>(r16));
            ::fast_io::fast_terminate();
        }
    }

    return 0;
}

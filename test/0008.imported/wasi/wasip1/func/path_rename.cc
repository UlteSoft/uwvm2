// path_rename wasm32 tests

#include <cstddef>
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_rename.h>

using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_bytes32(native_memory_t& memory, wasi_void_ptr_t p, void const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

inline static void write_cu8str32(native_memory_t& memory, wasi_void_ptr_t p, char8_t const* s)
{
    write_bytes32(memory, p, s, ::std::char_traits<char8_t>::length(s));
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

    constexpr wasi_void_ptr_t P0{1024u};
    constexpr wasi_void_ptr_t P1{4096u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(-1),
                                                                            static_cast<wasi_void_ptr_t>(0u),
                                                                            static_cast<wasi_size_t>(0u),
                                                                            static_cast<wasi_posix_fd_t>(3),
                                                                            static_cast<wasi_void_ptr_t>(0u),
                                                                            static_cast<wasi_size_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf)
        {
            ::fast_io::io::perrln("pr32 Case0", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Common dirfds at 3 (old) and 4 (new) with full rights
    set_dirfd(env, 3uz, static_cast<rights_t>(-1));
    set_dirfd(env, 4uz, static_cast<rights_t>(-1));

    // Case 1: missing right_path_rename_source on old -> enotcapable
    {
        set_dirfd(env, 5uz, static_cast<rights_t>(0));
        write_cu8str32(memory, P0, u8"a.txt");
        write_cu8str32(memory, P1, u8"b.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(5),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(sizeof(u8"a.txt") - 1u),
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(sizeof(u8"b.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("pr32 Case1", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2a: old fd is file -> enotdir
    {
        auto& f = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        f.rights_base = static_cast<rights_t>(-1);
        f.rights_inherit = static_cast<rights_t>(-1);
        f.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str32(memory, P0, u8"x");
        write_cu8str32(memory, P1, u8"y");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(6),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(1u),
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("pr32 Case2a", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2b: new fd is file -> enotdir
    {
        auto& f = *env.fd_storage.opens.index_unchecked(7uz).fd_p;
        f.rights_base = static_cast<rights_t>(-1);
        f.rights_inherit = static_cast<rights_t>(-1);
        f.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str32(memory, P0, u8"x");
        write_cu8str32(memory, P1, u8"y");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(3),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(1u),
                                                                            static_cast<wasi_posix_fd_t>(7),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("pr32 Case2b", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: empty old_path -> einval
    {
        write_cu8str32(memory, P1, u8"dst");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(3),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(0u),
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(3u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pr32 Case3", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: absolute path -> eperm
    {
        write_cu8str32(memory, P0, u8"/abs_old");
        write_cu8str32(memory, P1, u8"/abs_new");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(3),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(sizeof(u8"/abs_old") - 1u),
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(sizeof(u8"/abs_new") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("pr32 Case4", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: successful rename within same directory
    {
        try_unlink(u8"uwvm_ut_pr32_dst.txt");
        try_unlink(u8"uwvm_ut_pr32_src.txt");
        try
        {
            ::fast_io::native_file f{u8"uwvm_ut_pr32_src.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[1]{std::byte{'Z'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str32(memory, P0, u8"uwvm_ut_pr32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pr32_dst.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_rename(env,
                                                                            static_cast<wasi_posix_fd_t>(3),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pr32_src.txt") - 1u),
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pr32_dst.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pr32 Case5", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_file f{u8"uwvm_ut_pr32_dst.txt", ::fast_io::open_mode::in};
            ::std::byte b{};
            auto p = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
            if(p != &b + 1 || b != ::std::byte{'Z'})
            {
                ::fast_io::io::perrln("pr32 verify", static_cast<unsigned>(ret));
                ::fast_io::fast_terminate();
            }
        }
        catch(::fast_io::error)
        {
            ::fast_io::io::perrln("pr32 open dst failed");
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_pr32_dst.txt");
        try_unlink(u8"uwvm_ut_pr32_src.txt");
    }

    return 0;
}

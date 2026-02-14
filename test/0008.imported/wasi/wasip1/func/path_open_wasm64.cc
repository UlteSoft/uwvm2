#include <cstddef>
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber_wasm64.h>
#ifdef UWVM_DLLIMPORT
# error "UWVM_DLLIMPORT existed"
#endif

#ifdef UWVM_WASM_SUPPORT_WASM1
# error "UWVM_WASM_SUPPORT_WASM1 existed"
#endif

#ifdef UWVM_AES_RST_ALL
# error "UWVM_AES_RST_ALL existed"
#endif

#ifdef UWVM_COLOR_RST_ALL
# error "UWVM_COLOR_RST_ALL existed"
#endif

#ifdef UWVM_WIN32_TEXTATTR_RST_ALL
# error "UWVM_WIN32_TEXTATTR_RST_ALL existed"
#endif

#ifdef UWVM_IMPORT_WASI
# error "UWVM_IMPORT_WASI existed"
#endif

#ifdef UWVM_IMPORT_WASI_WASIP1
# error "UWVM_IMPORT_WASI_WASIP1 existed"
#endif

#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
// NOLINTBEGIN

using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
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
{
    write_bytes64(memory, p, s, ::std::char_traits<char8_t>::length(s));
}

inline static wasi_posix_fd_wasm64_t read_fd64(native_memory_t& memory, wasi_void_ptr_wasm64_t p)
{
    return ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, p);
}

inline static void set_dirfd(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_t base_rights, rights_t inherit_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = inherit_rights;
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

// helpers for cleanup per test case
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

inline static void try_rmdir(char8_t const* name)
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
    memory.init_by_page_count(6uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {.fd_limit = 64uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table
    env.fd_storage.opens.resize(32uz);

    // Pre-create inputs
    try
    {
        ::fast_io::native_file f{u8"uwvm_ut_po64_src.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        ::std::byte const data[3]{std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
        ::fast_io::operations::write_all_bytes(f, data, data + 3);
    }
    catch(::fast_io::error)
    {
    }

    // memory offsets
    constexpr wasi_void_ptr_wasm64_t P0{1024u};
    constexpr wasi_void_ptr_wasm64_t P1{4096u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P2{8192u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P3{12288u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P4{16384u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P5{20480u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P6{24576u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P7{28672u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P8{32768u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P9{36864u};
    constexpr wasi_void_ptr_wasm64_t PFD{45056u};

    // ===== Case 0: negative dirfd -> ebadf =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 static_cast<wasi_void_ptr_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf) { ::fast_io::fast_terminate(); }
    }

    // ===== Common dirfd at 3 with '.' and full rights (incl. right_path_open) =====
    set_dirfd(env, 3uz, static_cast<rights_t>(-1), static_cast<rights_t>(-1));

    // ===== Case 1: dirfd is file -> enotdir =====
    {
        auto& f = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        f.rights_base = static_cast<rights_t>(-1);
        f.rights_inherit = static_cast<rights_t>(-1);
        f.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 2: missing right_path_open on dirfd -> enotcapable =====
    {
        set_dirfd(env, 5uz, static_cast<rights_t>(0), static_cast<rights_t>(0));
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 3: empty path -> einval =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P1,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 4: invalid UTF-8 (contains zero) -> eilseq =====
    {
        unsigned char const bad_bytes[3]{'A', 0x00u, 'B'};
        write_bytes64(memory, P0, bad_bytes, 3u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(3u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 5: absolute path -> enotcapable =====
    {
        write_cu8str64(memory, P0, u8"/abs_po64");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"/abs_po64") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 6a: final '.' and open as dir -> esuccess =====
    {
        write_cu8str64(memory, P0, u8".");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8".") - 1u),
                                                                                 oflags_wasm64_t::o_directory,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 6b: final '.' and open as file -> eisdir =====
    {
        write_cu8str64(memory, P0, u8".");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8".") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 7a: final '..' and dirfd path_stack empty -> enotcapable (dir) =====
    {
        write_cu8str64(memory, P0, u8"..");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"..") - 1u),
                                                                                 oflags_wasm64_t::o_directory,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 7b: final '..' and open as file -> eisdir =====
    {
        write_cu8str64(memory, P0, u8"..");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"..") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 8: non-existent file without creat -> enoent =====
    {
        write_cu8str64(memory, P0, u8"po64_not_exist.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_not_exist.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
           && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval
#endif
        )
        {
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 9a: create new file with O_CREAT|O_EXCL -> esuccess and exists =====
    {
        try_unlink(u8"po64_create_a.txt");
        write_cu8str64(memory, P0, u8"po64_create_a.txt");
        auto const ofl = static_cast<oflags_wasm64_t>(static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_creat) |
                                                      static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_excl));
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_create_a.txt") - 1u),
                                                                                 ofl,
                                                                                 rights_wasm64_t::right_fd_write,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        // verify exists
        try
        {
            ::fast_io::native_file f{u8"po64_create_a.txt", ::fast_io::open_mode::in};
        }
        catch(::fast_io::error)
        {
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"po64_create_a.txt");
    }

    // ===== Case 9b: O_EXCL without O_CREAT -> einval =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 oflags_wasm64_t::o_excl,
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 9c: O_TRUNC requires write right -> einval =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 oflags_wasm64_t::o_trunc,
                                                                                 rights_wasm64_t::right_fd_read,  // no write
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 10: request base rights not subset of dirfd -> enotcapable =====
    {
        set_dirfd(env, 6uz, rights_t::right_path_open, rights_t::right_path_open);
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(6),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_write,  // not subset
                                                                                 rights_wasm64_t::right_fd_write,  // not subset
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 11: open existing file for read -> esuccess, then read via host to verify exists =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        ::fast_io::native_file f{u8"uwvm_ut_po64_src.txt", ::fast_io::open_mode::in};
        ::std::byte buf[3]{};
        auto p = ::fast_io::operations::read_some_bytes(f, buf, buf + 3);
        if(p != buf + 3 || buf[0] != ::std::byte{'A'} || buf[1] != ::std::byte{'B'} || buf[2] != ::std::byte{'C'}) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 12: directory flag with file path -> enotdir =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                 oflags_wasm64_t::o_directory,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir) { ::fast_io::fast_terminate(); }
    }

    // Prepare directory and nested path
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"po64_dir");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_file f{u8"po64_dir/f.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        ::std::byte const d[1]{std::byte{'Z'}};
        ::fast_io::operations::write_all_bytes(f, d, d + 1);
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 13: open directory with o_directory -> esuccess =====
    {
        write_cu8str64(memory, P0, u8"po64_dir");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_dir") - 1u),
                                                                                 oflags_wasm64_t::o_directory,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 14: traverse to nested file and open for read -> esuccess =====
    {
        write_cu8str64(memory, P0, u8"po64_dir/f.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_dir/f.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 15: intermediate is file -> enotdir =====
    {
        try
        {
            ::fast_io::native_file f{u8"po64_mid_file", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str64(memory, P0, u8"po64_mid_file/x");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_mid_file/x") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir) { ::fast_io::fast_terminate(); }
        try_unlink(u8"po64_mid_file");
    }

    // ===== Symlink-related tests on platforms that support symlink semantics =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    // Prepare: file and symlink
    try
    {
        ::fast_io::native_file f{u8"po64_tA.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        ::std::byte const d[1]{std::byte{'Q'}};
        ::fast_io::operations::write_all_bytes(f, d, d + 1);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"po64_tA.txt", ::fast_io::at_fdcwd(), u8"po64_lA");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 16a: final symlink nofollow (open file) -> eloop =====
    {
        write_cu8str64(memory, P0, u8"po64_lA");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),  // nofollow
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_lA") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 16b: final symlink follow -> esuccess =====
    {
        write_cu8str64(memory, P0, u8"po64_lA");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 lookupflags_wasm64_t::lookup_symlink_follow,
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_lA") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 17: intermediate symlink to directory -> open file under it =====
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"po64_realD");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_file f{u8"po64_realD/x.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        ::std::byte const d[1]{std::byte{'X'}};
        ::fast_io::operations::write_all_bytes(f, d, d + 1);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"po64_realD", ::fast_io::at_fdcwd(), u8"po64_linkD");
    }
    catch(::fast_io::error)
    {
    }
    {
        write_cu8str64(memory, P0, u8"po64_linkD/x.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_linkD/x.txt") - 1u),
                                                                                 static_cast<oflags_wasm64_t>(0),
                                                                                 rights_wasm64_t::right_fd_read,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 18: self-loop symlink follow -> eloop; nofollow -> enotdir =====
    try
    {
        ::fast_io::native_symlinkat(u8"po64_loop", ::fast_io::at_fdcwd(), u8"po64_loop");
    }
    catch(::fast_io::error)
    {
    }
    {
        write_cu8str64(memory, P0, u8"po64_loop");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                lookupflags_wasm64_t::lookup_symlink_follow,
                                                                                P0,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_loop") - 1u),
                                                                                oflags_wasm64_t::o_directory,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<fdflags_wasm64_t>(0),
                                                                                PFD);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop) { ::fast_io::fast_terminate(); }

        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                static_cast<lookupflags_wasm64_t>(0),
                                                                                P0,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_loop") - 1u),
                                                                                oflags_wasm64_t::o_directory,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<fdflags_wasm64_t>(0),
                                                                                PFD);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 19: symlink to absolute path; follow -> enotcapable; nofollow -> enotdir =====
    try
    {
        ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"po64_absL");
    }
    catch(::fast_io::error)
    {
    }
    {
        write_cu8str64(memory, P0, u8"po64_absL");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                lookupflags_wasm64_t::lookup_symlink_follow,
                                                                                P0,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_absL") - 1u),
                                                                                oflags_wasm64_t::o_directory,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<fdflags_wasm64_t>(0),
                                                                                PFD);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable) { ::fast_io::fast_terminate(); }

        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                static_cast<lookupflags_wasm64_t>(0),
                                                                                P0,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_absL") - 1u),
                                                                                oflags_wasm64_t::o_directory,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<fdflags_wasm64_t>(0),
                                                                                PFD);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir) { ::fast_io::fast_terminate(); }
    }

    // cleanup symlink fixtures
    try_unlink(u8"po64_tA.txt");
    try_unlink(u8"po64_lA");
    try_unlink(u8"po64_linkD");
    try_unlink(u8"po64_realD/x.txt");
    try_rmdir(u8"po64_realD");
    try_unlink(u8"po64_loop");
    try_unlink(u8"po64_absL");
#endif

    // ===== Case 20: fdflags invalid combos on dir (append|trunc|excl|creat) -> einval =====
    {
        write_cu8str64(memory, P0, u8"po64_dir");
        auto const ofl = static_cast<oflags_wasm64_t>(static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_directory) |
                                                      static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_trunc));
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<lookupflags_wasm64_t>(0),
                                                                                 P0,
                                                                                 static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_dir") - 1u),
                                                                                 ofl,
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<rights_wasm64_t>(0),
                                                                                 static_cast<fdflags_wasm64_t>(0),
                                                                                 PFD);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }

    // ===== Windows/DOS invalid-character tests (path syntax pre-checks) =====
#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    {
        write_cu8str64(memory, P0, u8"po64_inv_bs_a\\b");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                               static_cast<lookupflags_wasm64_t>(0),
                                                                               P0,
                                                                               static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_inv_bs_a\\b") - 1u),
                                                                               static_cast<oflags_wasm64_t>(0),
                                                                               rights_wasm64_t::right_fd_read,
                                                                               static_cast<rights_wasm64_t>(0),
                                                                               static_cast<fdflags_wasm64_t>(0),
                                                                               PFD);
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }
    {
        write_cu8str64(memory, P0, u8"po64_star_*");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                               static_cast<lookupflags_wasm64_t>(0),
                                                                               P0,
                                                                               static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_star_*") - 1u),
                                                                               static_cast<oflags_wasm64_t>(0),
                                                                               rights_wasm64_t::right_fd_read,
                                                                               static_cast<rights_wasm64_t>(0),
                                                                               static_cast<fdflags_wasm64_t>(0),
                                                                               PFD);
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }
#endif

    // ===== Case 21: open with neither read nor write (path mode) -> esuccess =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                               static_cast<lookupflags_wasm64_t>(0),
                                                                               P0,
                                                                               static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                               static_cast<oflags_wasm64_t>(0),
                                                                               static_cast<rights_wasm64_t>(0),
                                                                               static_cast<rights_wasm64_t>(0),
                                                                               static_cast<fdflags_wasm64_t>(0),
                                                                               PFD);
        if(!(r == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 22: O_DSYNC / O_RSYNC feature gates =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");

#ifndef O_DSYNC
        fdflags_wasm64_t ds = fdflags_wasm64_t::fdflag_dsync;
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                static_cast<lookupflags_wasm64_t>(0),
                                                                                P0,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                static_cast<oflags_wasm64_t>(0),
                                                                                rights_wasm64_t::right_fd_read,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                ds,
                                                                                PFD);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup) { ::fast_io::fast_terminate(); }
#endif

#ifndef O_RSYNC
        fdflags_wasm64_t rs = fdflags_wasm64_t::fdflag_rsync;
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                static_cast<lookupflags_wasm64_t>(0),
                                                                                P0,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                static_cast<oflags_wasm64_t>(0),
                                                                                rights_wasm64_t::right_fd_read,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                rs,
                                                                                PFD);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup) { ::fast_io::fast_terminate(); }
#endif
    }

    // ===== Case 23: fd_close - close once success, double-close -> ebadf =====
    {
        write_cu8str64(memory, P0, u8"uwvm_ut_po64_src.txt");
        auto const r_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    static_cast<lookupflags_wasm64_t>(0),
                                                                                    P0,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"uwvm_ut_po64_src.txt") - 1u),
                                                                                    static_cast<oflags_wasm64_t>(0),
                                                                                    rights_wasm64_t::right_fd_read,
                                                                                    static_cast<rights_wasm64_t>(0),
                                                                                    static_cast<fdflags_wasm64_t>(0),
                                                                                    PFD);
        if(r_open != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const fd_val = static_cast<wasi_posix_fd_wasm64_t>(read_fd64(memory, PFD));
        auto const r_close1 = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_val);
        if(r_close1 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const r_close2 = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_val);
        if(r_close2 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf) { ::fast_io::fast_terminate(); }
    }

    // ===== Case 24a: fd_renumber to new index -> old ebadf, new close esuccess =====
    {
        try_unlink(u8"po64_rnA.txt");
        write_cu8str64(memory, P0, u8"po64_rnA.txt");
        auto const ofl = static_cast<oflags_wasm64_t>(static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_creat) |
                                                      static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_excl));
        auto const r_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    static_cast<lookupflags_wasm64_t>(0),
                                                                                    P0,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_rnA.txt") - 1u),
                                                                                    ofl,
                                                                                    rights_wasm64_t::right_fd_write,
                                                                                    static_cast<rights_wasm64_t>(0),
                                                                                    static_cast<fdflags_wasm64_t>(0),
                                                                                    PFD);
        if(r_open != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const fd_from = static_cast<wasi_posix_fd_wasm64_t>(read_fd64(memory, PFD));
        auto const fd_to = static_cast<wasi_posix_fd_wasm64_t>(fd_from + 100);
        auto const r_ren = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, fd_from, fd_to);
        if(r_ren != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const r_close_old = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_from);
        if(r_close_old != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf) { ::fast_io::fast_terminate(); }
        auto const r_close_new = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_to);
        if(r_close_new != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        try_unlink(u8"po64_rnA.txt");
    }

    // ===== Case 24b: fd_renumber overwrite existing fd slot =====
    {
        try_unlink(u8"po64_rnB.txt");
        try_unlink(u8"po64_rnC.txt");

        auto const ofl = static_cast<oflags_wasm64_t>(static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_creat) |
                                                      static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_excl));

        // open B
        write_cu8str64(memory, P0, u8"po64_rnB.txt");
        auto const r_open_b = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                      static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                      static_cast<lookupflags_wasm64_t>(0),
                                                                                      P0,
                                                                                      static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_rnB.txt") - 1u),
                                                                                      ofl,
                                                                                      rights_wasm64_t::right_fd_write,
                                                                                      static_cast<rights_wasm64_t>(0),
                                                                                      static_cast<fdflags_wasm64_t>(0),
                                                                                      PFD);
        if(r_open_b != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const fd_b = static_cast<wasi_posix_fd_wasm64_t>(read_fd64(memory, PFD));

        // open C
        write_cu8str64(memory, P0, u8"po64_rnC.txt");
        auto const r_open_c = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                      static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                      static_cast<lookupflags_wasm64_t>(0),
                                                                                      P0,
                                                                                      static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_rnC.txt") - 1u),
                                                                                      ofl,
                                                                                      rights_wasm64_t::right_fd_write,
                                                                                      static_cast<rights_wasm64_t>(0),
                                                                                      static_cast<fdflags_wasm64_t>(0),
                                                                                      PFD);
        if(r_open_c != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const fd_c = static_cast<wasi_posix_fd_wasm64_t>(read_fd64(memory, PFD));

        // renumber B -> C (overwrite C)
        auto const r_ren = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, fd_b, fd_c);
        if(r_ren != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

        // close C succeeds, B now invalid
        if(::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_c) != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::fast_terminate();
        }
        if(::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_b) != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf)
        {
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"po64_rnB.txt");
        try_unlink(u8"po64_rnC.txt");
    }

    // ===== Case 26: fd_limit - emfile on exceed, success after close =====
    {
        // compute current effective count
        auto const open_size = env.fd_storage.opens.size();
        auto const close_size = env.fd_storage.closes.size();
        auto const renum_size = env.fd_storage.renumber_map.size();
        auto const all_size = open_size - close_size + renum_size;

        auto const prev_limit = env.fd_storage.fd_limit;
        env.fd_storage.fd_limit = all_size + 1;  // allow one more, then fail

        // open one new file OK
        try_unlink(u8"po64_fdlim_ok.txt");
        write_cu8str64(memory, P0, u8"po64_fdlim_ok.txt");
        auto const ofl = static_cast<oflags_wasm64_t>(static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_creat) |
                                                      static_cast<::std::underlying_type_t<oflags_wasm64_t>>(oflags_wasm64_t::o_excl));
        auto const r_ok = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                  static_cast<lookupflags_wasm64_t>(0),
                                                                                  P0,
                                                                                  static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_fdlim_ok.txt") - 1u),
                                                                                  ofl,
                                                                                  rights_wasm64_t::right_fd_write,
                                                                                  static_cast<rights_wasm64_t>(0),
                                                                                  static_cast<fdflags_wasm64_t>(0),
                                                                                  PFD);
        if(r_ok != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const fd_ok = static_cast<wasi_posix_fd_wasm64_t>(read_fd64(memory, PFD));

        // next open should fail with EMFILE
        try_unlink(u8"po64_fdlim_fail.txt");
        write_cu8str64(memory, P0, u8"po64_fdlim_fail.txt");
        auto const r_fail = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                    static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                    static_cast<lookupflags_wasm64_t>(0),
                                                                                    P0,
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_fdlim_fail.txt") - 1u),
                                                                                    ofl,
                                                                                    rights_wasm64_t::right_fd_write,
                                                                                    static_cast<rights_wasm64_t>(0),
                                                                                    static_cast<fdflags_wasm64_t>(0),
                                                                                    PFD);
        if(r_fail != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile) { ::fast_io::fast_terminate(); }

        // close one and open again should succeed
        if(::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_ok) != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"po64_fdlim_ok2.txt");
        write_cu8str64(memory, P0, u8"po64_fdlim_ok2.txt");
        auto const r_ok2 = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                   static_cast<lookupflags_wasm64_t>(0),
                                                                                   P0,
                                                                                   static_cast<wasi_size_wasm64_t>(sizeof(u8"po64_fdlim_ok2.txt") - 1u),
                                                                                   ofl,
                                                                                   rights_wasm64_t::right_fd_write,
                                                                                   static_cast<rights_wasm64_t>(0),
                                                                                   static_cast<fdflags_wasm64_t>(0),
                                                                                   PFD);
        if(r_ok2 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }
        auto const fd_ok2 = static_cast<wasi_posix_fd_wasm64_t>(read_fd64(memory, PFD));
        if(::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_ok2) != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::fast_terminate();
        }

        // restore limit and cleanup files
        env.fd_storage.fd_limit = prev_limit;
        try_unlink(u8"po64_fdlim_ok.txt");
        try_unlink(u8"po64_fdlim_ok2.txt");
        try_unlink(u8"po64_fdlim_fail.txt");
    }

    // Final cleanup
    try_unlink(u8"uwvm_ut_po64_src.txt");
    try_unlink(u8"po64_dir/f.txt");
    try_rmdir(u8"po64_dir");

    return 0;
}

// NOLINTEND

#else

int main() {}

#endif

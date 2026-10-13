// Source-only actual FreeBSD 15.1 SDK/kernel oracle. Run only in the disposable
// owned guest, under the shared 64 GiB outer cgroup. A cross-built ELF alone is
// not a platform, filesystem-durability or whole-checkpoint qualification.
#if defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <bit>
#include <array>
#include <fast_io.h>

// Every scalar/kernel borrow uses the actual target SDK type and an explicit
// noexcept ELF assembler symbol; no guessed Linux/BSD layout or C++ C-function
// exception assumption is introduced. The real ELF version bindings are kept
// by the fixed cross-build readobj stage before actual guest execution.
extern "C" int uwvm_test_xuname(int, void*) noexcept __asm__("__xuname");
extern "C" int uwvm_test_fstat(int, struct stat*) noexcept __asm__("fstat");
extern "C" int uwvm_test_fsync(int) noexcept __asm__("fsync");
extern "C" uid_t uwvm_test_getuid() noexcept __asm__("getuid");
extern "C" uid_t uwvm_test_geteuid() noexcept __asm__("geteuid");

namespace
{
[[nodiscard]] bool require(bool success, char const* description)
{
    if(!success) { ::fast_io::perrln("FAIL ", ::fast_io::mnp::os_c_str(description)); }
    return success;
}
template<::std::size_t N>
[[nodiscard]] auto bounded_kernel_text(char const (&text)[N]) noexcept
{
    ::std::size_t size{};
    // [owned SDK char[N]] one-past
    // [safe            ] never dereference at N;
    //  ^^ prove size<N before each complete kernel-output byte read.
    while(size < N && text[size] != '\0') { ++size; }
    return ::fast_io::basic_io_scatter_t<char>{text, size};
}
[[nodiscard]] bool parse_format_model()
{
    ::std::array<char, 8u> const decimal{'4', '2', '9', '4', '9', '6', '7', '2'};
    ::std::uint64_t value{};
    // [owned decimal8] one-past
    // [safe         ] endpoint only, never read;
    //  ^^ extent8 proves first+size before the scanner receives this range.
    char const* const first{decimal.data()};
    char const* const last{first + decimal.size()};
    auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(value))};
    return parsed.code == ::fast_io::parse_code::ok && parsed.iter == last && value == 42949672u;
}
}
int main()
{
    static_assert(sizeof(off_t) == 8u && sizeof(void*) == 8u);
    if(!require(uwvm_test_getuid() != 0u && uwvm_test_geteuid() != 0u, "actual probe must execute as the owned non-root guest user")) { return 1; }
    struct utsname kernel{};
    // [complete owned SDK utsname] end
    // [safe                    ] synchronous __xuname borrows the full record;
    //  ^^ the actual SDK SYS_NMLN is passed, no record-pointer arithmetic.
    if(!require(uwvm_test_xuname(SYS_NMLN, __builtin_addressof(kernel)) == 0, "actual __xuname kernel provider")) { return 1; }
    ::fast_io::println("kernel.sysname=", bounded_kernel_text(kernel.sysname), "; release=", bounded_kernel_text(kernel.release),
        "; machine=", bounded_kernel_text(kernel.machine), "; sdk.version=", ::fast_io::mnp::dec(__FreeBSD_version),
        "; native.endian=", ::fast_io::mnp::os_c_str(::std::endian::native == ::std::endian::big ? "big" : "little"));
    ::fast_io::println("sdk.stat.size=", sizeof(struct stat), "; align=", alignof(struct stat),
        "; dev@", offsetof(struct stat, st_dev), "; ino@", offsetof(struct stat, st_ino), "; nlink@", offsetof(struct stat, st_nlink),
        "; mode@", offsetof(struct stat, st_mode), "; uid@", offsetof(struct stat, st_uid), "; gid@", offsetof(struct stat, st_gid),
        "; rdev@", offsetof(struct stat, st_rdev), "; atim@", offsetof(struct stat, st_atim), "; mtim@", offsetof(struct stat, st_mtim),
        "; ctim@", offsetof(struct stat, st_ctim), "; birthtim@", offsetof(struct stat, st_birthtim), "; size@", offsetof(struct stat, st_size),
        "; blocks@", offsetof(struct stat, st_blocks), "; blksize@", offsetof(struct stat, st_blksize), "; flags@", offsetof(struct stat, st_flags),
        "; gen@", offsetof(struct stat, st_gen), "; filerev@", offsetof(struct stat, st_filerev),
        "; off_t.size=", sizeof(off_t), "; time_t.size=", sizeof(time_t), "; uid_t.size=", sizeof(uid_t));
    if(!require(parse_format_model(), "actual public fast_io decimal scanner")) { return 1; }
    {
        ::fast_io::native_file file{"fastio-oracle.bin", ::fast_io::open_mode::in | ::fast_io::open_mode::out |
            ::fast_io::open_mode::creat | ::fast_io::open_mode::excl, static_cast<::fast_io::perms>(0600u)};
        struct stat native{};
        // [complete owned SDK stat] end
        // [safe                  ] synchronous fstat borrows exactly this record;
        //  ^^ no caller pointer is advanced; the target SDK fixes its layout.
        if(!require(uwvm_test_fstat(file.native_handle(), __builtin_addressof(native)) == 0 &&
            S_ISREG(native.st_mode) && native.st_size == 0, "actual SDK fstat sees new owned empty regular file")) { return 1; }
        auto const wide{::fast_io::operations::io_stream_seek_bytes(file, static_cast<::fast_io::intfpos_t>(8ULL << 30), ::fast_io::seekdir::beg)};
        if(!require(wide == (8ULL << 30) && ::fast_io::status(file).size == 0u, "actual 64-bit seek does not allocate or enlarge the file")) { return 1; }
        if(!require(::fast_io::operations::io_stream_seek_bytes(file, 0, ::fast_io::seekdir::beg) == 0u, "owned file seeks back to origin")) { return 1; }
        ::fast_io::print(file, ::fast_io::mnp::le_put<64>(0x0123456789abcdefULL), ::fast_io::mnp::leb128_put(624485u));
        if(!require(uwvm_test_fsync(file.native_handle()) == 0, "actual BSD scalar file sync succeeds")) { return 1; }
        if(!require(uwvm_test_fstat(file.native_handle(), __builtin_addressof(native)) == 0 && native.st_size == 11, "actual fstat sees canonical LE+LEB payload size")) { return 1; }
        auto const portable{::fast_io::status(file)};
        if(!require(portable.dev == native.st_dev && portable.ino == native.st_ino && portable.uid == native.st_uid &&
            portable.gid == native.st_gid && portable.nlink == native.st_nlink && portable.size == 11u &&
            portable.type == ::fast_io::file_type::regular, "actual fast_io status agrees with independently declared SDK fstat")) { return 1; }
        ::fast_io::println("actual.fast_io.status=", portable);
        file.close();
    }
    // The private fixture file has no concurrent writer, and all write owners
    // were checked-closed before this mapped loader borrows the actual bytes.
    ::fast_io::native_file_loader mapping{"fastio-oracle.bin", ::fast_io::open_mode::in};
    if(!require(mapping.size() == 11u, "actual native_file_loader maps all eleven canonical bytes")) { return 1; }
    ::std::array<unsigned char, 11u> const expected{0xefu, 0xcdu, 0xabu, 0x89u, 0x67u, 0x45u, 0x23u, 0x01u, 0xe5u, 0x8eu, 0x26u};
    for(::std::size_t index{}; index != expected.size(); ++index)
    {
        // [owned mapping exactly11 bytes] end
        // [safe                         ] index<11 for this full-byte access;
        //  ^^ size==11 and index!=11 are proved above, no endpoint dereference.
        if(!require(static_cast<unsigned char>(mapping[index]) == expected[index], "actual canonical LE/LEB file bytes")) { return 1; }
    }
    ::fast_io::println("PASS FreeBSD SDK/kernel and fast_io native_file/status/64-bit-seek/mapped-loader/decimal/LE/LEB;",
        " filesystem durability/VM continuation/ROS LLVM23 product acceptance=false");
}
#else
int main() { return 77; }
#endif

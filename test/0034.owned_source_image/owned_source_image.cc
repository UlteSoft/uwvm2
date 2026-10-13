#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <uwvm2/utils/control/owned_file_image.h>
namespace
{
    namespace image = ::uwvm2::utils::control;
    void require(bool condition, unsigned line)
    {
        if(condition) { return; }
        ::fast_io::print(::fast_io::err(), "owned_source_image FAIL line=", ::fast_io::mnp::dec(line), "\n");
        ::fast_io::fast_terminate();
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    auto path(char const* value)
    { return ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(value))); }
}
int main(int argc, char** argv)
{
    // Keeper creates the actual binary from a Core3 fixture with custom payload,
    // plus actual empty/large/dir/missing/symlink cases. No synthetic provider.
    if(argc != 8) { return 64; }
    static_assert(!::std::is_default_constructible_v<image::owned_file_image>);
    static_assert(!::std::is_copy_constructible_v<image::owned_file_image>);
    static_assert(::std::is_same_v<decltype(::std::declval<image::owned_file_image const&>().bytes()[0]), ::std::byte const&>);
    auto retained{image::owned_file_image::read(path(argv[1]), 1048576uz)};
    REQUIRE(retained && retained.image->size() >= 8uz && retained.image->observed_status().type == ::fast_io::file_type::regular);
    auto const original{retained.image->bytes()};
    auto const saved_size{original.size()};
    ::std::uint_least64_t checksum{};
    for(auto const byte: original) { checksum = checksum * 131u + ::std::to_integer<unsigned>(byte); }
    auto const pointer{retained.image->cbegin()};
    auto failed{image::owned_file_image::read(path(argv[1]), 1uz)};
    REQUIRE(!failed && !failed.image && failed.error == image::owned_image_error::quota_exceeded);
    REQUIRE(retained.image->cbegin() == pointer && retained.image->size() == saved_size);
    auto invalid{image::owned_file_image::read(::std::u8string{u8"bad\0path", 8uz}, 1uz)};
    REQUIRE(!invalid && invalid.error == image::owned_image_error::invalid_path);
    auto invalid_limit{image::owned_file_image::read(path(argv[1]), image::owned_file_image::maximum_bytes + 1uz)};
    REQUIRE(!invalid_limit && invalid_limit.error == image::owned_image_error::quota_exceeded);
    auto empty{image::owned_file_image::read(path(argv[2]))};
    REQUIRE(!empty && empty.error == image::owned_image_error::empty_file);
    auto directory{image::owned_file_image::read(path(argv[3]))};
    REQUIRE(!directory && (directory.error == image::owned_image_error::not_regular || directory.error == image::owned_image_error::open_failed));
    auto missing{image::owned_file_image::read(path(argv[4]))};
    REQUIRE(!missing && missing.error == image::owned_image_error::open_failed);
    auto symlink{image::owned_file_image::read(path(argv[5]))};
    REQUIRE(!symlink && (symlink.error == image::owned_image_error::open_failed || symlink.error == image::owned_image_error::not_regular));
    auto large{image::owned_file_image::read(path(argv[6]), 1024uz)};
    REQUIRE(!large && large.error == image::owned_image_error::quota_exceeded);
    // Real truncation AFTER load must not mutate the byte owner or cause mapped
    // SIGBUS. File IO/output goes through fast_io native RAII only.
    { ::fast_io::native_file truncate{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::out | ::fast_io::open_mode::trunc}; }
    ::std::uint_least64_t after{};
    for(auto const byte: retained.image->bytes()) { after = after * 131u + ::std::to_integer<unsigned>(byte); }
    REQUIRE(after == checksum && retained.image->size() == saved_size && retained.image->cbegin() == pointer);
    // Also overwrite through a real reopened file; no previous writable host
    // mapping or restored external file is claimed by this owned-byte test.
    { ::fast_io::obuf_file output{::fast_io::mnp::os_c_str(argv[1])}; ::fast_io::print(output, "replacement host bytes"); ::fast_io::flush(output); }
    after = 0u;
    for(auto const byte: retained.image->bytes()) { after = after * 131u + ::std::to_integer<unsigned>(byte); }
    REQUIRE(after == checksum && retained.image->cbegin() == pointer);
    auto moved{::std::move(retained.image)};
    REQUIRE(!retained.image && moved && moved->cbegin() == pointer && moved->size() == saved_size);
    // The keeper compares stdout digest/size with its independent original
    // fixture bytes and proves the changed host file really differs afterward.
    ::fast_io::print(::fast_io::out(), "OWNED_SOURCE_IMAGE bytes=", ::fast_io::mnp::dec(saved_size), " checksum=",
        ::fast_io::mnp::dec(checksum), " truncate_overwrite_stable=1 owner_move=1 failed_new_load_preserved_old=1\n");
    ::fast_io::obuf_file evidence{::fast_io::mnp::os_c_str(argv[7])};
    // Export owned compiler DATA through buffered fast_io bytes. The output is
    // a component oracle, not an isolated/transactional checkpoint database.
    // [immutable private bytes.data(), size()] one-past
    // [safe: true owner size>=8 and size<=PTRDIFF_MAX] ^^ end formed after
    // live owner/extent proof; synchronous fast_io borrows this exact range.
    ::fast_io::operations::write_all_bytes(evidence, moved->bytes().data(), moved->bytes().data() + moved->size());
    ::fast_io::flush(evidence);
    return 0;
}

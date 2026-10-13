// Source-owner negative component. No fake initializer or source receipt.
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <fast_io_unit/string.h>
# include <uwvm2/utils/control/owned_file_image.h>
# include <uwvm2/uwvm/runtime/storage/source_digest.h>
#else
import fast_io;
import fast_io_crypto;
import uwvm2.utils.control;
import uwvm2.uwvm.runtime.storage;
#endif
namespace full = ::uwvm2::uwvm::runtime::full;
namespace control = ::uwvm2::utils::control;
static void require(bool ok, unsigned line)
{
    if(!ok) { ::fast_io::io::perrln("owned source digest DATA FAIL line=", ::fast_io::mnp::dec(line)); ::fast_io::fast_terminate(); }
}
#define REQUIRE(x) require(bool(x), __LINE__)
static ::std::u8string path(char const* value)
{ return ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(value))); }
static auto actual_owned_source(char const* name)
{
    auto image{control::owned_file_image::read(path(name), 1048576uz)};
    REQUIRE(image && image.image->size() >= 8u);
    auto source{full::full_source_instance::create_unparsed(path(name))};
    REQUIRE(source->file_for_native_initialization().adopt_unparsed_source_image(::std::move(image.image)));
    return source;
}
// Hash this actual retained private image only as independent byte DATA. It is
// NOT the production initialized-source receipt, whose positive still needs
// real parser+initializer+seal and the current selected module/phase.
static auto raw_image_data(full::full_source_instance::owner const& source)
{
    auto const& file{source->file()}; auto const size{file.source_size()};
    REQUIRE(file.has_owned_source_image() && size >= 8u && size <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()));
    auto const* first{reinterpret_cast<::std::byte const*>(file.source_cbegin())}; REQUIRE(first != nullptr);
    ::fast_io::sha256_context sha{};
    // [actual private immutable image, size<=PTRDIFF and nonnull] end
    // [safe] checked extent BEFORE first+size; no parser/VM authority.
    sha.update(first, first + size); sha.do_final(); ::std::array<::std::byte, 32u> result{};
    sha.digest_to_byte_ptr(result.data()); return result;
}
int main(int argc, char** argv)
{
    if(argc != 3) { return 64; }
    REQUIRE(full::digest_actual_owned_source({}).status == full::source_digest_status::foreign_source_owner);
    auto empty{full::full_source_instance::create_unparsed(path(argv[1]))};
    REQUIRE(full::digest_actual_owned_source(empty).status == full::source_digest_status::unavailable_immutable_origin);
    auto mapped{full::full_source_instance::create_unparsed(path(argv[1]))};
    mapped->file_for_native_initialization().wasm_file = ::fast_io::native_file_loader{::fast_io::mnp::os_c_str(argv[1]),
        ::fast_io::open_mode::in | ::fast_io::open_mode::follow};
    REQUIRE(mapped->file().source_size() >= 8u && !mapped->file().has_owned_source_image());
    REQUIRE(full::digest_actual_owned_source(mapped).status == full::source_digest_status::unavailable_immutable_origin);
    auto a{actual_owned_source(argv[1])}; auto b{actual_owned_source(argv[2])};
    auto ia{full::digest_actual_owned_source(a)}; auto ib{full::digest_actual_owned_source(b)};
    REQUIRE(ia.status == full::source_digest_status::unsealed_native_source && !ia.identity);
    REQUIRE(ib.status == full::source_digest_status::unsealed_native_source && !ib.identity);
    // A distinct control block around the same pointer is still not canonical.
    // Its genuine owner remains alive; no invalid dereference/deletion occurs.
    full::full_source_instance::owner foreign{a.get(), [](auto const*) noexcept {}};
    REQUIRE(full::digest_actual_owned_source(foreign).status == full::source_digest_status::foreign_source_owner);
    auto const ah{raw_image_data(a)}, bh{raw_image_data(b)}; REQUIRE(ah != bh);
    ::fast_io::io::print("OWNED_SOURCE_DIGEST source_data_only=1 original_bytes=", ::fast_io::mnp::dec(a->file().source_size()), " sha256=");
    for(auto byte : ah) { ::fast_io::io::print(::fast_io::mnp::hex<false, true>(::std::to_integer<unsigned char>(byte))); }
    ::fast_io::io::println(" unsealed_refused=1 initialized_source_positive=0 core3_validation=0 loaded_build_cache_issuer=0 whole_restore=0");
}

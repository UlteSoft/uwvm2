// PRIVATE source-only candidate: original byte identity DATA, no VM authority.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <optional>
# include <utility>
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/control/owned_file_image.h>
# include "full.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::full
{
    enum class source_digest_status : unsigned char
    { ok, foreign_source_owner, unavailable_immutable_origin, unsealed_native_source, invalid_owned_extent, unsupported_character_width };
    class owned_source_digest;
    struct owned_source_digest_result;
    [[nodiscard]] owned_source_digest_result digest_actual_owned_source(full_source_instance::owner const& source) noexcept;
    // A cold source identity pins exactly the owner whose private image was
    // hashed after its actual initializer sealed the file. It proves only original
    // byte identity, not full validation/build/engine or VM capture/restore.
    class owned_source_digest final
    {
        full_source_instance::owner source_{};
        ::std::uint64_t bytes_{};
        ::std::array<::std::byte, 32u> digest_{};
        owned_source_digest(full_source_instance::owner source, ::std::uint64_t bytes,
            ::std::array<::std::byte, 32u> digest) noexcept
            : source_{::std::move(source)}, bytes_{bytes}, digest_{digest} {}
        friend owned_source_digest_result digest_actual_owned_source(full_source_instance::owner const&) noexcept;
    public:
        owned_source_digest(owned_source_digest const&) = default;
        owned_source_digest& operator=(owned_source_digest const&) = default;
        owned_source_digest(owned_source_digest&&) noexcept = default;
        owned_source_digest& operator=(owned_source_digest&&) noexcept = default;
        [[nodiscard]] full_source_instance::owner const& source_owner() const noexcept { return source_; }
        [[nodiscard]] ::std::uint64_t source_bytes() const noexcept { return bytes_; }
        [[nodiscard]] ::std::array<::std::byte, 32u> const& original_sha256() const noexcept { return digest_; }
        [[nodiscard]] constexpr bool grants_capture_or_restore_authority() const noexcept { return false; }
    };
    struct owned_source_digest_result
    {
        source_digest_status status{source_digest_status::foreign_source_owner};
        ::std::optional<owned_source_digest> identity{};
    };
    [[nodiscard]] inline owned_source_digest_result digest_actual_owned_source(full_source_instance::owner const& source) noexcept
    {
        // Native setup/publication only, with a valid, live native source
        // pointee from the actual factory. has_canonical_owner reads that
        // pointee; it is NOT an arbitrary-address registry admission oracle.
        // The future private publisher must first compare its genuine runtime
        // source registry owner (get/control block), then invoke this helper.
        // Caller also holds existing serialized source-selection ownership;
        // no parser/image adoption or escaped setup borrow may race this call.
        if constexpr(CHAR_BIT != 8) { return {source_digest_status::unsupported_character_width, {}}; }
        if(!full_source_instance::has_canonical_owner(source))
        { return {source_digest_status::foreign_source_owner, {}}; }
        auto const& file{source->file()}; // canonical strong owner retains SAME nonmoving file
        if(!file.has_owned_source_image())
        { return {source_digest_status::unavailable_immutable_origin, {}}; }
        // Before the actual seal, file_for_native_initialization() can replace
        // the entire file. A source control block alone cannot pin that image.
        // This checks the real private initializer/main-module/serial state,
        // never a supplied ready bool or a detached numeric admission flag.
        if(!source->initialized_from_actual_state())
        { return {source_digest_status::unsealed_native_source, {}}; }
        auto const size{file.source_size()};
        if(size < 8u || size > ::uwvm2::utils::control::owned_file_image::maximum_bytes ||
           size > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
        { return {source_digest_status::invalid_owned_extent, {}}; }
        auto const* first{reinterpret_cast<::std::byte const*>(file.source_cbegin())};
        if(first == nullptr) { return {source_digest_status::invalid_owned_extent, {}}; }
        ::fast_io::sha256_context sha{};
        // [SAME private initialized immutable image: first ... size] exclusive_end
        // [safe] nonnull, size>=8, actual-image maximum and PTRDIFF bound BEFORE +.
        // The entire original image (custom/data sections included) is hashed;
        // no path reopen, parser body hint, mapped-file or borrowed-span fallback.
        sha.update(first, first + size);
        sha.do_final(); ::std::array<::std::byte, 32u> digest{};
        sha.digest_to_byte_ptr(digest.data()); // fixed complete owned 32-byte destination; no advance
        // Factory maximum is 1 GiB, proving this uint64 conversion even on a
        // hypothetical wider size_t. This copy retains the canonical source.
        return {source_digest_status::ok, owned_source_digest{source, static_cast<::std::uint64_t>(size), digest}};
    }
}

// Cold owned-byte/source DATA fixture; no parser, guest start or VM issuer.
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_unit/string.h>
# include <uwvm2/utils/control/owned_file_image.h>
# include <uwvm2/uwvm/runtime/storage/full.h>
#else
import fast_io;
import uwvm2.utils.control;
import uwvm2.uwvm.runtime.storage;
#endif
namespace
{
    namespace control = ::uwvm2::utils::control;
    namespace full = ::uwvm2::uwvm::runtime::full;
    void require(bool condition, unsigned line)
    {
        if(condition) { return; }
        ::fast_io::io::perrln("owned byte copy DATA FAIL line=", ::fast_io::mnp::dec(line));
        ::fast_io::fast_terminate();
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    control::owned_image_result copy_expiring_input()
    {
        ::std::array<::std::byte, 8uz> input{::std::byte{0}, ::std::byte{0x61}, ::std::byte{0x73}, ::std::byte{0x6d},
            ::std::byte{1}, ::std::byte{0}, ::std::byte{0}, ::std::byte{0}};
        // The stack allocation expires on return. The new image retains only
        // its distinct initialized allocation; this is not parsed Wasm proof.
        return control::owned_file_image::copy_owned_bytes(input, input.size());
    }
}
int main()
{
    using image_type = control::owned_file_image;
    static_assert(!::std::is_default_constructible_v<image_type>);
    static_assert(!::std::is_copy_constructible_v<image_type>);
    static_assert(!::std::is_move_constructible_v<image_type>);
    static_assert(::std::is_same_v<image_type::owner, ::std::unique_ptr<image_type const>>);
    static_assert(::std::is_same_v<decltype(::std::declval<image_type const&>().bytes()[0]), ::std::byte const&>);
    static_assert(noexcept(image_type::copy_owned_bytes(::std::span<::std::byte const>{})));
    ::std::array<::std::byte, 256uz> input{};
    for(::std::size_t index{}; index != input.size(); ++index)
    {
        // [0,256) actual initialized array; index bounded BEFORE write.
        input[index] = static_cast<::std::byte>(index);
    }
    ::std::span<::std::byte const> alias{input};
    auto first{image_type::copy_owned_bytes(alias, input.size())};
    REQUIRE(first && first.image->size() == input.size());
    REQUIRE(first.image->bytes().data() != input.data());
    REQUIRE(first.image->observed_status().type == ::fast_io::file_type::none);
    REQUIRE(first.image->observed_status().size == 0u && first.image->observed_status().ino == 0u);
    auto const* original{first.image->cbegin()};
    // Mutate the retained caller allocation after the synchronous copy; both
    // the caller span alias and this array now differ from the immutable image.
    input.fill(::std::byte{0});
    for(::std::size_t index{}; index != first.image->size(); ++index)
    {
        // [actual immutable image, size256] end; bounded BEFORE indexed read.
        REQUIRE(first.image->bytes()[index] == static_cast<::std::byte>(index));
    }
    auto second{image_type::copy_owned_bytes(first.image->bytes(), first.image->size())};
    REQUIRE(second && second.image->bytes().data() != first.image->bytes().data());
    for(::std::size_t index{}; index != first.image->size(); ++index)
    { REQUIRE(second.image->bytes()[index] == first.image->bytes()[index]); }
    auto too_small{image_type::copy_owned_bytes(alias, input.size() - 1uz)};
    REQUIRE(!too_small && !too_small.image && too_small.error == control::owned_image_error::quota_exceeded);
    auto zero_limit{image_type::copy_owned_bytes(alias, 0uz)};
    REQUIRE(!zero_limit && !zero_limit.image && zero_limit.error == control::owned_image_error::quota_exceeded);
    auto excessive_limit{image_type::copy_owned_bytes(alias, image_type::maximum_bytes + 1uz)};
    REQUIRE(!excessive_limit && !excessive_limit.image && excessive_limit.error == control::owned_image_error::quota_exceeded);
    auto empty{image_type::copy_owned_bytes({})};
    REQUIRE(!empty && !empty.image && empty.error == control::owned_image_error::empty_file);
    REQUIRE(first.image->cbegin() == original && first.image->size() == 256uz);
    auto moved{::std::move(first.image)};
    REQUIRE(!first.image && moved && moved->cbegin() == original);
    auto expired{copy_expiring_input()};
    REQUIRE(expired && expired.image->size() == 8uz && expired.image->bytes()[1uz] == ::std::byte{0x61});
    auto source{full::full_source_instance::create_unparsed(::std::u8string{u8"checkpoint-byte-copy-DATA"})};
    REQUIRE(full::full_source_instance::has_canonical_owner(source));
    REQUIRE(source->file_for_native_initialization().adopt_unparsed_source_image(::std::move(moved)));
    REQUIRE(!moved && source->file().has_owned_source_image());
    REQUIRE(source->file().source_cbegin() == original && source->file().source_size() == 256uz);
    // Image uses exclusive ownership, so it has no shared control block to
    // trust. The real source root DOES have one: a different block around the
    // same valid live pointer must fail its actual canonical-owner check.
    full::full_source_instance::owner foreign{source.get(), [](auto const*) noexcept {}};
    REQUIRE(!full::full_source_instance::has_canonical_owner(foreign));
    REQUIRE(!source->initialized_from_actual_state());
    REQUIRE(!source->seal_actual_initializer());
    REQUIRE(!source->bind_actual_compiled_main(1uz, nullptr));
    REQUIRE(!source->file_for_native_initialization().adopt_unparsed_source_image(::std::move(second.image)));
    REQUIRE(source->file().source_cbegin() == original && source->file().source_size() == 256uz);
    ::fast_io::io::println("OWNED_BYTE_IMAGE copy=1 caller_alias_mutation=1 input_expiry=1 distinct_allocations=1",
        " immutable_unique_owner=1 failed_copy_preserved_old=1 canonical_source_control=1",
        " observed_file_status=0 parser_validation=0 initialized_source=0 whole_restore=0");
}

// SOURCE ONLY. Mandatory kernel-origin authentication for persistent objects.
#pragma once
#ifndef UWVM_MODULE
#include <array>
#include <cstddef>
#include <fast_io_crypto.h>
#include "kernel_origin.h"

#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache::origin_auth
{
    // HMAC-SHA256(random kernel-only key, fixed domain || public context seed ||
    // self-image digest). No derived Ed25519 private seed survives this object.
    class signing_seed final
    {
        kernel_origin::secret_bytes<32u> bytes_{};
        bool ready_{};
    public:
        signing_seed() noexcept = default;
        signing_seed(signing_seed const&) = delete;
        signing_seed& operator=(signing_seed const&) = delete;
        template<typename PublicSeed, typename ImageDigest>
        [[nodiscard]] bool derive(runtime_origin::use const& actual, PublicSeed const& public_seed,
            ImageDigest const& image) noexcept
        {
            if(ready_ || public_seed.size() != 32u || image.size() != 32u || !kernel_origin::dumping_disabled() ||
               !runtime_origin::secret_reads_allowed(actual)) { return false; }
            kernel_origin::secret_bytes<32u> secret{};
            if(!kernel_origin::read(actual, secret)) { return false; }
            kernel_origin::secret_bytes<64u> inner_pad{}, outer_pad{};
            for(::std::size_t index{}; index != 64u; ++index)
            {
                auto const key{index < 32u ? secret.value[index] : ::std::byte{}};
                inner_pad.value[index] = key ^ ::std::byte{0x36u};
                outer_pad.value[index] = key ^ ::std::byte{0x5cu};
            }
            ::fast_io::sha256_context inner{};
            inner.update(inner_pad.value.data(), inner_pad.value.data() + inner_pad.value.size());
            constexpr char domain[]{"uwvm-llvm-jit-origin-ed25519-v7\0"};
            auto const* domain_begin{reinterpret_cast<::std::byte const*>(domain)};
            inner.update(domain_begin, domain_begin + sizeof(domain) - 1u);
            inner.update(public_seed.data(), public_seed.data() + public_seed.size());
            inner.update(image.data(), image.data() + image.size());
            inner.do_final();
            kernel_origin::secret_bytes<32u> intermediate{};
            inner.digest_to_byte_ptr(intermediate.value.data());
            ::fast_io::sha256_context outer{};
            outer.update(outer_pad.value.data(), outer_pad.value.data() + outer_pad.value.size());
            outer.update(intermediate.value.data(), intermediate.value.data() + intermediate.value.size());
            outer.do_final(); outer.digest_to_byte_ptr(bytes_.value.data());
            // sha256_context itself contains chaining state and buffered bytes.
            // Wipe both after final extraction, not merely the explicit key pads.
            static_assert(::std::is_trivially_destructible_v<decltype(inner)>);
            kernel_origin::erase(::std::addressof(inner), sizeof(inner));
            kernel_origin::erase(::std::addressof(outer), sizeof(outer));
            ready_ = kernel_origin::dumping_disabled() && runtime_origin::secret_reads_allowed(actual);
            if(!ready_) { kernel_origin::erase(bytes_.value.data(), bytes_.value.size()); }
            return ready_;
        }
        [[nodiscard]] unsigned char const* data() const noexcept
        { return ready_ ? reinterpret_cast<unsigned char const*>(bytes_.value.data()) : nullptr; }
        [[nodiscard]] bool ready() const noexcept { return ready_; }
    };
}

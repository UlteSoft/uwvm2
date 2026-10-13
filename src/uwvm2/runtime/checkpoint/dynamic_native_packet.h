/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "shadow_ledger.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    // Independently owned typed native DATA, not capture/restore authority.
    // A runtime producer must first prove actual compiler frame provenance,
    // canonical publication/generation and private pause/root ownership before
    // it forms these bounded spans. Native ABI bits NEVER enter the database.
    class dynamic_native_packet
    {
        sealed_function_plan::owner plan_{};
        ::std::uint64_t site_{};
        ::std::vector<native_value> values_{};
        dynamic_native_packet(sealed_function_plan::owner plan, ::std::uint64_t site,
            ::std::vector<native_value>&& values) : plan_{::std::move(plan)}, site_{site}, values_{::std::move(values)} {}
    public:
        using owner = ::std::shared_ptr<dynamic_native_packet const>;
        [[nodiscard]] sealed_function_plan::owner const& plan() const noexcept { return plan_; }
        [[nodiscard]] ::std::uint64_t site() const noexcept { return site_; }
        [[nodiscard]] ::std::span<native_value const> values() const noexcept { return values_; }
        [[nodiscard]] static status copy_compiler_packet(sealed_function_plan::owner plan, ::std::uint64_t site,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> actual_local_flags, owner& result)
        {
            ::std::vector<native_value> detached{};
            auto const decoded{shadow_ledger::decode_compiler_packet(plan,site,payload,actual_local_flags,detached)};
            if(decoded!=status::ok) { return decoded; }
            owner candidate{new dynamic_native_packet{::std::move(plan), site, ::std::move(detached)}};
            result.swap(candidate); return status::ok; // no partial publication on rejection/allocation failure
        }
        [[nodiscard]] status executable_restore_capability() const noexcept { return status::unavailable_resume; }
    };
}

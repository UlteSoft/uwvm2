// This compares retained actual RC identities during private preparation.
// It grants no stop, capture or resource authority and never serializes pointers.
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
#include <map>
#include <optional>
#include <span>
#endif
namespace uwvm2::runtime::lib::wasip1_resource_identity
{
    // Each row represents ONE saved resource, not one guest binding. Several
    // bindings can share a row; distinct rows must retain distinct target RCs.
    [[nodiscard]] inline ::std::optional<::std::size_t> conflict(
        ::std::span<::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_ref_t const> resources)
    {
        ::std::map<void const*,::std::size_t> owners{};
        for(::std::size_t i{};i!=resources.size();++i)
        {
            auto const identity{resources[i].ptr};
            if(identity==nullptr || !owners.emplace(identity,i).second) { return i; }
        }
        return ::std::nullopt;
    }
}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <uwvm2/utils/thread/execution_domain.h>
#include <uwvm2/uwvm/runtime/initializer/init_limit.h>

namespace uwvm2::runtime::lib { class runtime_threaded_profile_bridge; }
namespace uwvm2::uwvm::runtime::full { class full_source_instance; }
namespace uwvm2::uwvm::runtime::storage { struct wasm_module_storage_t; }

namespace uwvm2::runtime::wasm_threads
{
    // Host policy DATA. Zero does not grant any threaded execution. The bridge
    // validates every finite bound and overflow BEFORE retaining a generation,
    // building a child world/code object or creating an OS thread.
    struct threaded_profile_limits
    {
        ::std::size_t max_active_children{};
        ::std::uint32_t max_spawn_attempts{};
        ::std::size_t native_stack_bytes{};
        // Total controlled mmap reservation, including two guard pages. This is
        // not an assertion that arbitrary LLVM recursion fits its usable part.
        ::std::size_t compiler_stack_bytes{};
        ::std::size_t max_live_native_stack_bytes{};
        ::std::size_t max_child_storage_bytes{};
        ::std::size_t max_live_storage_bytes{};
        ::std::size_t max_child_code_bytes{};
        ::std::size_t max_live_code_bytes{};
        ::std::size_t max_table_elements{};
        ::std::size_t max_local_memory_bytes{};
        ::std::size_t max_shared_memory_bytes{};
        ::std::size_t max_total_segment_bytes{};
        ::std::size_t max_value_stack_bytes{};
        ::std::size_t max_control_frames{};
        ::std::size_t max_call_frames{};
        ::uwvm2::uwvm::runtime::initializer::initializer_limit_t initializer{};
    };

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // This is a source-generation/provider lifetime pin, NOT a restore token,
    // a permission boolean, a copied epoch or a public Guest capability. Only
    // the actual Runtime bridge may create it, after checking canonical source,
    // validation epoch, explicit policy, provider ownership and real admission.
    // Group/world/code retain this same owner until ALL actual native children
    // have physically joined. Runtime update/reset must first retire the group.
    class ThreadedSpawnAdmission final
    {
        friend class ::uwvm2::runtime::lib::runtime_threaded_profile_bridge;
        using parent_owner = ::std::shared_ptr<::uwvm2::uwvm::runtime::full::full_source_instance const>;
        ::uwvm2::utils::thread::execution_domain::lease provider_generation_;
        parent_owner source_;
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module_{};
        ::std::size_t module_id_{};
        ::std::uint_least64_t generation_{}, validation_epoch_{};
        threaded_profile_limits limits_{};
        ::std::weak_ptr<ThreadedSpawnAdmission const> canonical_owner_;

        ThreadedSpawnAdmission(parent_owner source,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
            ::std::size_t module_id, ::std::uint_least64_t generation,
            ::std::uint_least64_t validation_epoch, threaded_profile_limits limits,
            ::uwvm2::utils::thread::execution_domain::lease actual_provider_generation) noexcept
            : provider_generation_{::std::move(actual_provider_generation)},
              source_{::std::move(source)}, module_{::std::addressof(module)},
              module_id_{module_id}, generation_{generation}, validation_epoch_{validation_epoch}, limits_{limits} {}

    public:
        ThreadedSpawnAdmission(ThreadedSpawnAdmission const&) = delete;
        ThreadedSpawnAdmission& operator=(ThreadedSpawnAdmission const&) = delete;
        [[nodiscard]] auto const& source_owner() const noexcept { return source_; }
        [[nodiscard]] auto const& source_module() const noexcept { return *module_; }
        [[nodiscard]] ::std::size_t source_module_id() const noexcept { return module_id_; }
        [[nodiscard]] ::std::uint_least64_t source_generation() const noexcept { return generation_; }
        [[nodiscard]] ::std::uint_least64_t source_validation_epoch() const noexcept { return validation_epoch_; }
        [[nodiscard]] auto const& initializer_limits() const noexcept { return limits_.initializer; }
        [[nodiscard]] auto const& limits() const noexcept { return limits_; }
        [[nodiscard]] ::std::size_t max_table_elements() const noexcept { return limits_.max_table_elements; }
        [[nodiscard]] ::std::size_t max_local_memory_bytes() const noexcept { return limits_.max_local_memory_bytes; }
        [[nodiscard]] ::std::size_t max_shared_memory_bytes() const noexcept { return limits_.max_shared_memory_bytes; }
        [[nodiscard]] ::std::size_t max_total_segment_bytes() const noexcept { return limits_.max_total_segment_bytes; }
        [[nodiscard]] ::std::shared_ptr<ThreadedSpawnAdmission const> retain() const noexcept
        { return canonical_owner_.lock(); }
        [[nodiscard]] bool owns_actual_generation() const noexcept
        { return static_cast<bool>(provider_generation_); }
    };
#else
    // An unsupported host has no admission constructor or usable spawn profile.
    class ThreadedSpawnAdmission;
#endif
}

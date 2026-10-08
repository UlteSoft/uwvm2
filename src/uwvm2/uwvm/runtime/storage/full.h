/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <atomic>
# include <string>
# include <type_traits>
# include <utility>
# include <vector>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/control/owned_file_image.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/runtime/gc/instance_phase.h>
# include "storage.h"
# include "builtin_wasip1_loader.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

namespace uwvm2::uwvm::runtime::initializer
{
    extern "C++" { class restoration_context; }
}
namespace uwvm2::runtime::lib
{
    extern "C++" { class runtime_checkpoint_world_transaction; }
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::full
{

    // Native storage owner. It exists before load_wasm_file constructs parser
    // objects; neither the file nor its inline Core3 context is ever moved.
    // Mutable metadata access is restricted to serialized, drained native setup.
    // This is not a guest handle, a numeric admission certificate or an epoch pin.
    // Pure typed weak-lifetime edge. This class supplies no plan authentication,
    // module pointer or optimization authority. Its real derived admitted_plan
    // remains owned and checked by the backend, avoiding storage/outer module
    // import cycles and avoiding a shared_ptr<void> admission credential.
    class numeric_plan_lifetime
    {
    protected:
        numeric_plan_lifetime() noexcept = default;
    public:
        virtual ~numeric_plan_lifetime() = default;
        numeric_plan_lifetime(numeric_plan_lifetime const&) = delete;
        numeric_plan_lifetime& operator=(numeric_plan_lifetime const&) = delete;
    };

    // Cold native loader INPUT only; no parsed pointers/IDs/epochs supplied.
    // Names, exact per-module feature policy and private immutable image all
    // move into the final source owner BEFORE any new parser view is created.
    struct full_preload_input
    {
        ::std::u8string file_name{}, module_name{};
        ::uwvm2::uwvm::wasm::type::wasm_parameter_t parameters{};
        ::uwvm2::utils::control::owned_file_image::owner image{};
    };

    class full_source_instance final
    {
        friend class ::uwvm2::uwvm::runtime::initializer::restoration_context;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;
        ::std::u8string file_name_storage_;
        ::std::u8string rename_storage_;
        ::uwvm2::uwvm::wasm::type::wasm_file_t file_{};
        // These arrays reach their FINAL extent before parsing. No later growth,
        // relocation, reassignment of names or parser files is allowed. An old
        // prepared code owner therefore retains its original parser/store graph
        // even after the selected source is retired and replaced.
        ::std::vector<full_preload_input> preload_inputs_{};
        ::uwvm2::utils::container::vector<::uwvm2::uwvm::wasm::type::wasm_file_t> preload_files_{};
        struct preload_binding
        {
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module{};
            ::std::size_t id{(::std::numeric_limits<::std::size_t>::max)()};
            ::std::uint_least64_t validation_epoch{};
        };
        mutable ::std::vector<preload_binding> preload_bindings_{};
        // Actual final source owns isolated declaration/export and rewritten-name
        // buffers beyond context teardown; maps are not global-selector credentials.
        decltype(::uwvm2::uwvm::wasm::storage::all_module) checkpoint_declarations_{};
        decltype(::uwvm2::uwvm::wasm::storage::all_module_export) checkpoint_exports_{};
        ::uwvm2::uwvm::wasm::storage::configured_module_import_reset_map_t checkpoint_import_resets_{};
        ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit_map_t checkpoint_memory_limits_{};
        ::uwvm2::uwvm::runtime::storage::runtime_registry_type registry_{};
        ::std::weak_ptr<full_source_instance const> canonical_owner_{};
        // Strong provenance identity ONLY. The native implementation remains
        // unique-owned by preload_local_imported; every read below is lexical
        // under genuine maintenance/closed-host/source admission, not lifetime
        // permission granted by this identity or a saved checkpoint record.
        ::std::shared_ptr<builtin_wasip1_loader_identity const> builtin_wasip1_identity_{};
        ::uwvm2::uwvm::wasm::type::local_imported_t const* builtin_wasip1_member_{};
        ::uwvm2::uwvm::wasm::type::local_imported_t const* builtin_wasip1_vector_begin_{};
        ::std::size_t builtin_wasip1_vector_extent_{};

        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* main_module_{};
        ::std::uint_least64_t initializer_serial_{};
        mutable ::std::size_t main_module_id_{(::std::numeric_limits<::std::size_t>::max)()};
        bool initialized_{};
        bool numeric_full_requested_{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        bool native_eh_private_leaf_requested_{};
#endif
        mutable bool full_validation_complete_{};
        mutable ::std::uint_least64_t validation_epoch_{};
        mutable ::std::weak_ptr<numeric_plan_lifetime const> numeric_plan_{};

        explicit full_source_instance(::std::u8string file_name, ::std::u8string rename,
            ::std::vector<full_preload_input> inputs)
            : file_name_storage_{::std::move(file_name)}, rename_storage_{::std::move(rename)}, preload_inputs_{::std::move(inputs)}
        {
            // [this nonmoving owner: terminated filename and rename buffers]
            // [safe] file_ borrows these final object buffers before parsing.
            file_.file_name = owned_file_name();
            file_.module_name = owned_rename();
            if(preload_inputs_.size() > 4095u) { ::fast_io::fast_terminate(); }
            preload_files_.resize(preload_inputs_.size()); preload_bindings_.resize(preload_inputs_.size());
            // FINAL array extents established BEFORE any filename borrow or parse.
            for(::std::size_t index{}; index != preload_files_.size(); ++index)
            {
                auto const& input{preload_inputs_[index]}; auto& file{preload_files_.index_unchecked(index)};
                file.file_name = decltype(file.file_name){::fast_io::containers::null_terminated, input.file_name.c_str(), input.file_name.size()};
                file.module_name = decltype(file.module_name){input.module_name.data(), input.module_name.size()};
                file.wasm_parameter = input.parameters;
            }
        }

    public:
        using mutable_owner = ::std::shared_ptr<full_source_instance>;
        using owner = ::std::shared_ptr<full_source_instance const>;
        full_source_instance(full_source_instance const&) = delete;
        full_source_instance& operator=(full_source_instance const&) = delete;
        full_source_instance(full_source_instance&&) = delete;
        full_source_instance& operator=(full_source_instance&&) = delete;
        ~full_source_instance() = default;

        [[nodiscard]] static mutable_owner create_unparsed(::std::u8string file_name, ::std::u8string rename = {},
            ::std::vector<full_preload_input> inputs = {})
        {
            // Real nonconst allocation, including both file/map objects. Bind the
            // weak control-block identity before any const owner is published.
            mutable_owner result{new full_source_instance{::std::move(file_name), ::std::move(rename), ::std::move(inputs)}};
            result->canonical_owner_ = result;
            return result;
        }

        [[nodiscard]] ::uwvm2::utils::container::u8cstring_view owned_file_name() const noexcept
        {
            return ::uwvm2::utils::container::u8cstring_view{
                ::fast_io::containers::null_terminated, file_name_storage_.c_str(), file_name_storage_.size()};
        }
        [[nodiscard]] ::uwvm2::utils::container::u8string_view owned_rename() const noexcept
        {
            // [safe] The storage owns exactly [data(), data() + size()).
            // This view borrows that range while this source owner lives;
            // no pointer is advanced and no byte outside the range is read.
            return ::uwvm2::utils::container::u8string_view{rename_storage_.data(), rename_storage_.size()};
        }

        // These native borrows cannot outlive the owner and must not escape setup.
        // The actual runtime maintenance callback excludes entries/metadata workers.
        [[nodiscard]] ::uwvm2::uwvm::wasm::type::wasm_file_t& file_for_native_initialization() noexcept
        {
            if(initialized_) { ::fast_io::fast_terminate(); }
            return file_;
        }
        [[nodiscard]] auto& preloaded_files_for_native_initialization() noexcept
        {
            if(initialized_) { ::fast_io::fast_terminate(); }
            return preload_files_;
        }
        [[nodiscard]] ::std::size_t preloaded_file_count() const noexcept { return preload_files_.size(); }
        [[nodiscard]] ::uwvm2::utils::control::owned_file_image::owner take_preloaded_image_for_native_initialization(
            ::std::size_t index) noexcept
        {
            if(initialized_ || index >= preload_inputs_.size() || index >= preload_files_.size() ||
               preload_files_.index_unchecked(index).binfmt_ver != 0u) { ::fast_io::fast_terminate(); }
            return ::std::move(preload_inputs_[index].image); // Before any parser borrows; no owner/file relocation.
        }
        [[nodiscard]] ::uwvm2::uwvm::runtime::storage::runtime_registry_type& registry_for_native_initialization() noexcept
        {
            if(initialized_) { ::fast_io::fast_terminate(); }
            return registry_;
        }
        [[nodiscard]] ::uwvm2::uwvm::wasm::type::wasm_file_t const& file() const noexcept { return file_; }
        [[nodiscard]] ::uwvm2::uwvm::runtime::storage::runtime_registry_type const& registry() const noexcept { return registry_; }

        [[nodiscard]] static bool has_canonical_owner(owner const& candidate) noexcept
        {
            if(!candidate) { return false; }
            auto const& weak{candidate->canonical_owner_};
            return !weak.owner_before(candidate) && !candidate.owner_before(weak);
        }

        // Called only after the actual initializer completed globals/tables and
        // published its real per-module phase. No caller supplied ready/serial bool.
        [[nodiscard]] bool seal_actual_initializer() noexcept
        {
            if(initialized_ || file_.binfmt_ver != 1u ||
               ::uwvm2::uwvm::wasm::storage::details::selected_execute_wasm != ::std::addressof(file_) ||
               ::uwvm2::uwvm::runtime::storage::details::selected_runtime_registry != ::std::addressof(registry_))
            { return false; }
            auto const serial{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)};
            auto const found{registry_.find(file_.module_name)};
            if(found == registry_.end() || !found->second.gc_collection_phase.initialized_for(serial)) { return false; }
            // [actual initialized member of this owned registry]
            // [safe] found is not end; main_module_ never names an external module.
            if(registry_.size() != preload_files_.size() + 1u || preload_files_.size() != preload_bindings_.size() ||
               ::uwvm2::uwvm::wasm::storage::details::selected_preloaded_wasm != ::std::addressof(preload_files_)) { return false; }
            for(::std::size_t index{}; index != preload_files_.size(); ++index)
            {
                // [final owned parser files][actual owned registry] end
                // [safe] full actual extent BEFORE indexed WF/type/phase read.
                auto const& file{preload_files_.index_unchecked(index)};
                auto const member{registry_.find(file.module_name)};
                auto const declared{::uwvm2::uwvm::wasm::storage::all_module.find(file.module_name)};
                if(file.binfmt_ver != 1u || member == registry_.end() || member == found ||
                   !member->second.gc_collection_phase.initialized_for(serial) ||
                   declared == ::uwvm2::uwvm::wasm::storage::all_module.end() ||
                   declared->second.type != ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm ||
                   declared->second.module_storage_ptr.wf != ::std::addressof(file)) { return false; }
                for(::std::size_t earlier{}; earlier != index; ++earlier)
                { if(preload_bindings_[earlier].module == ::std::addressof(member->second)) { return false; } }
                preload_bindings_[index].module = ::std::addressof(member->second);
            }
            // Real initializer owns this drained native setup. The factory
            // identity must be current and bound to the FINAL native vector
            // and typed unified registry, before any source epoch is sealed.
            auto const builtin_identity{builtin_wasip1_loader_identity::selected_};
            if(builtin_identity)
            {
                if(!builtin_wasip1_loader_identity::canonical(builtin_identity)) { return false; }
                auto const* member{builtin_identity->current_member()};
                if(member == nullptr) { return false; }
                builtin_wasip1_identity_=builtin_identity;builtin_wasip1_member_=member;
                auto const& providers{::uwvm2::uwvm::wasm::storage::preload_local_imported};
                builtin_wasip1_vector_begin_=providers.cbegin();builtin_wasip1_vector_extent_=providers.size();
            }
            main_module_ = ::std::addressof(found->second);
            initializer_serial_ = serial;
            initialized_ = true;
            // Native source loading is finished; normal CLI reload cannot mutate
            // the selected parser file in place after Core3 addresses escaped.
            ::uwvm2::uwvm::wasm::storage::details::selected_execute_wasm_initialized = true;
            return true;
        }
        [[nodiscard]] bool initialized_from_actual_state() const noexcept
        {
            return initialized_ && main_module_ != nullptr &&
                main_module_->gc_collection_phase.initialized_for(initializer_serial_);
        }

        // Cold actual dense-record builder only, under the runtime publication lock.
        // The integer is an observed assigned ID; it retains no resource by itself.
        [[nodiscard]] bool bind_actual_compiled_main(::std::size_t assigned_id,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(!initialized_from_actual_state() || actual_module != main_module_ ||
                assigned_id == (::std::numeric_limits<::std::size_t>::max)()) { return false; }
            main_module_id_ = assigned_id;
            return true;
        }
        [[nodiscard]] bool bind_actual_compiled_module(::std::size_t assigned_id,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(!initialized_from_actual_state() || assigned_id == (::std::numeric_limits<::std::size_t>::max)()) { return false; }
            if(actual_module == main_module_) { return bind_actual_compiled_main(assigned_id, actual_module); }
            for(auto& binding : preload_bindings_)
            {
                // Supplied address is only compared until it matches our actual
                // initializer-sealed member, never dereferenced as caller authority.
                if(binding.module == actual_module) { binding.id = assigned_id; return true; }
            }
            return false;
        }
        [[nodiscard]] ::std::size_t bound_initialized_module_id(
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(!initialized_from_actual_state()) { return (::std::numeric_limits<::std::size_t>::max)(); }
            if(actual_module == main_module_) { return main_module_id_; }
            for(auto const& binding : preload_bindings_)
            { if(binding.module == actual_module) { return binding.id; } }
            return (::std::numeric_limits<::std::size_t>::max)();
        }
        [[nodiscard]] ::uwvm2::uwvm::wasm::type::wasm_file_t const* bound_initialized_file(::std::size_t actual_id,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(actual_id == (::std::numeric_limits<::std::size_t>::max)() || !initialized_from_actual_state()) { return nullptr; }
            if(actual_module == main_module_)
            { return actual_id == main_module_id_ && file_.binfmt_ver == 1u ? ::std::addressof(file_) : nullptr; }
            if(preload_bindings_.size() != preload_files_.size()) { return nullptr; }
            for(::std::size_t index{}; index != preload_bindings_.size(); ++index)
            {
                auto const& binding{preload_bindings_[index]};
                if(binding.module != actual_module || binding.id != actual_id) { continue; }
                // Genuine initializer-sealed module address + actual dense ID
                // matched BEFORE any module/owned WF pointee is dereferenced.
                if(!binding.module->gc_collection_phase.initialized_for(initializer_serial_)) { return nullptr; }
                auto const& file{preload_files_.index_unchecked(index)};
                return file.binfmt_ver == 1u ? ::std::addressof(file) : nullptr;
            }
            return nullptr;
        }
        [[nodiscard]] ::uwvm2::uwvm::wasm::type::wasm_file_t const* actual_validated_file(::std::size_t actual_id,
            ::std::uint_least64_t actual_epoch,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(actual_epoch == 0u) { return nullptr; }
            auto const file{bound_initialized_file(actual_id, actual_module)};
            if(file == nullptr) { return nullptr; }
            if(actual_module == main_module_) { return full_validation_complete_ && validation_epoch_ == actual_epoch ? file : nullptr; }
            for(auto const& binding : preload_bindings_)
            { if(binding.module == actual_module && binding.id == actual_id) { return binding.validation_epoch == actual_epoch ? file : nullptr; } }
            return nullptr;
        }
        // Cold debug-byte exclusivity qualification ONLY. This authenticates
        // the actual finalized native provider census for this current source;
        // no copied boolean supplies pause/host/N/publication or byte rights.
        [[nodiscard]] bool actual_no_unadapted_native_memory_provider(::std::size_t actual_id,
            ::std::uint_least64_t actual_epoch,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(actual_validated_file(actual_id,actual_epoch,actual_module)==nullptr) { return false; }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            // Previously delivered C ABI tables/descriptors cannot be revoked
            // by the current exposure flag or an empty counted host gate.
            if(!::uwvm2::uwvm::wasm::storage::preloaded_dl.empty()) { return false; }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            if(!::uwvm2::uwvm::wasm::storage::weak_symbol.empty()) { return false; }
#endif
            auto const& providers{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            if(!builtin_wasip1_identity_) { return providers.empty(); }
            if(!builtin_wasip1_loader_identity::canonical(builtin_wasip1_identity_)) { return false; }
            auto const& selected{builtin_wasip1_loader_identity::selected_};
            if(!selected || selected.get()!=builtin_wasip1_identity_.get() ||
               selected.owner_before(builtin_wasip1_identity_) || builtin_wasip1_identity_.owner_before(selected) ||
               providers.size()!=builtin_wasip1_vector_extent_ || providers.cbegin()!=builtin_wasip1_vector_begin_ ||
               builtin_wasip1_identity_->current_member()!=builtin_wasip1_member_) { return false; }
            for(::std::size_t i{};i!=providers.size();++i)
            {
                // [actual unchanged FINAL native vector][i<count] end
                // [safe] compare its OWN member address, not a name/count or
                // caller pointer. Only the original factory-certified builtin
                // wrapper is exempt; actual host-import cache still refuses
                // its byte operations without an independent real adapter.
                if(::std::addressof(providers.index_unchecked(i))!=builtin_wasip1_member_) { return false; }
            }
            return true;
        }
        // Cold lexical builtin binding, not an external-resource restorer.
        // The caller must retain genuine maintenance/ONE-hostclose/N/epoch
        // protection. Only DATA is returned; no host pointer survives here.
        // Passing labels, pointers or a detached descriptor grants no rights.
        [[nodiscard]] bool actual_builtin_wasip1_function(::std::size_t actual_id,
            ::std::uint_least64_t actual_epoch,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module,
            ::std::size_t actual_import_index, builtin_wasip1_function_data& output) const noexcept
        {
            if(actual_validated_file(actual_id, actual_epoch, actual_module) == nullptr ||
               !builtin_wasip1_loader_identity::canonical(builtin_wasip1_identity_)) { return false; }
            auto const& current{builtin_wasip1_loader_identity::selected_};
            if(!current || current.get() != builtin_wasip1_identity_.get() ||
               current.owner_before(builtin_wasip1_identity_) || builtin_wasip1_identity_.owner_before(current)) { return false; }
            auto const& providers{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            if(providers.size() != builtin_wasip1_vector_extent_ || providers.cbegin() != builtin_wasip1_vector_begin_ ||
               builtin_wasip1_identity_->current_member() != builtin_wasip1_member_) { return false; }
            // actual_validated_file authenticated this real owned registry
            // member BEFORE any module dereference. Bound the import ordinal
            // BEFORE its record read; an opaque supplied host pointer is never
            // used as source authority or called through a virtual table.
            auto const& imports{actual_module->imported_function_vec_storage};
            if(actual_import_index >= imports.size()) { return false; }
            auto const& native{imports.index_unchecked(actual_import_index)};
            if(native.link_kind != ::uwvm2::uwvm::runtime::storage::imported_function_link_kind::local_imported ||
               native.target.local_imported.module_ptr != builtin_wasip1_member_) { return false; }
            return builtin_wasip1_identity_->copy_function(builtin_wasip1_member_,native.target.local_imported.index,output);
        }
        [[nodiscard]] ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* initialized_main_module() const noexcept
        { return initialized_from_actual_state() ? main_module_ : nullptr; }
        // Concrete initialized_native_generation implementation. The caller is
        // the real full registry/validator publisher under its existing lock.
        // A request bit/epoch alone proves neither module identity nor lifetime.
        [[nodiscard]] static bool pending_numeric_has_canonical_owner(owner const& candidate) noexcept
        { return has_canonical_owner(candidate); }
        [[nodiscard]] ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*
            pending_numeric_initialized_module() const noexcept
        { return full_validation_complete_ && initialized_from_actual_state() ? main_module_ : nullptr; }
        [[nodiscard]] ::std::size_t pending_numeric_module_id() const noexcept
        { return full_validation_complete_ ? main_module_id_ : (::std::numeric_limits<::std::size_t>::max)(); }
        // Compiler declaration identity from the actual initializer and bound
        // dense record. This accessor supplies no function-validation proof.
        [[nodiscard]] ::std::size_t bound_initialized_main_module_id() const noexcept
        {
            return initialized_from_actual_state() ? main_module_id_ :
                (::std::numeric_limits<::std::size_t>::max)();
        }
        [[nodiscard]] ::std::uint_least64_t actual_full_validation_epoch() const noexcept
        { return full_validation_complete_ ? validation_epoch_ : 0u; }

        // Only the real validated full-record publisher invokes this method.
        // It cannot accept a foreign descriptor, caller bool or guessed ID.
        [[nodiscard]] bool record_actual_full_validation(::std::size_t actual_id,
            ::std::uint_least64_t actual_epoch,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module) const noexcept
        {
            if(actual_epoch == 0u || bound_initialized_file(actual_id, actual_module) == nullptr) { return false; }
            if(actual_module == main_module_)
            {
                validation_epoch_ = actual_epoch; full_validation_complete_ = true; return true;
            }
            for(auto& binding : preload_bindings_)
            {
                if(binding.module == actual_module && binding.id == actual_id)
                { binding.validation_epoch = actual_epoch; return true; }
            }
            return false;
        }
        [[nodiscard]] bool pending_numeric_requested() const noexcept { return numeric_full_requested_; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        [[nodiscard]] bool native_eh_private_leaf_requested() const noexcept { return native_eh_private_leaf_requested_; }
        [[nodiscard]] bool request_native_eh_private_leaf_after_actual_initializer() noexcept
        {
            // Administrative request only, before compiler workers/code exist.
            // An actual initialized source remains necessary; this bit supplies
            // no fused-validation, native CFI or executable permission.
            if(!initialized_from_actual_state() || full_validation_complete_) { return false; }
            native_eh_private_leaf_requested_ = true;
            return true;
        }
#endif
        void request_numeric_full_after_actual_initializer() noexcept
        {
            if(!initialized_from_actual_state()) { ::fast_io::fast_terminate(); }
            // Optimization request only, never an admission/owner certificate.
            numeric_full_requested_ = true;
        }
        void bind_numeric_plan_lifetime_after_actual_validation(
            ::std::shared_ptr<numeric_plan_lifetime const> const& actual_plan) const noexcept
        {
            if(!full_validation_complete_ || !actual_plan) { ::fast_io::fast_terminate(); }
            numeric_plan_ = actual_plan; // weak only: no source->plan->source cycle
        }
        void clear_numeric_plan_lifetime_after_drain() const noexcept { numeric_plan_.reset(); }
        [[nodiscard]] bool has_numeric_plan_lifetime() const noexcept { return !numeric_plan_.expired(); }
        [[nodiscard]] bool numeric_plan_lifetime_matches(
            ::std::shared_ptr<numeric_plan_lifetime const> const& actual_plan) const noexcept
        {
            // Cold publication observer only: this edge is lifetime evidence,
            // not admission. The caller separately checks the actual typed
            // admitted_plan and its concrete canonical owning-source pin.
            return actual_plan && !numeric_plan_.owner_before(actual_plan) && !actual_plan.owner_before(numeric_plan_);
        }
        void retire_actual_full_code_after_drain() const noexcept
        {
            numeric_plan_.reset();
            full_validation_complete_ = false;
            validation_epoch_ = 0u;
            for(auto& binding : preload_bindings_) { binding.validation_epoch = 0u; }
        }
        [[nodiscard]] ::std::size_t assigned_main_module_id() const noexcept { return main_module_id_; }
    };

    static_assert(!::std::is_copy_constructible_v<full_source_instance>);
    static_assert(!::std::is_move_constructible_v<full_source_instance>);
    static_assert(!::std::is_move_assignable_v<full_source_instance>);

    namespace details
    {
        // Strong native publication root. The two lower-layer selectors are mere
        // borrows into this object; they are never evidence of ownership/admission.
        inline full_source_instance::owner selected_full_source{};
    }

    // Caller holds native maintenance/publication ownership or the existing
    // externally serialized administration contract. This is not an atomic
    // getter for a host observer racing replacement without an execution lease.
    [[nodiscard]] inline full_source_instance::owner selected_full_source_owner_pin() noexcept
    { return details::selected_full_source; } // cold native setup/materialization only

    [[nodiscard]] inline bool select_unparsed_full_source_after_drain(
        full_source_instance::mutable_owner const& candidate) noexcept
    {
        full_source_instance::owner const owner{candidate};
        if(!full_source_instance::has_canonical_owner(owner) || candidate->initialized_from_actual_state()) { return false; }
        if(details::selected_full_source) { return false; }
        details::selected_full_source = owner;
        // [strong selected_full_source -> nonmoving actual file/map]
        // [safe] selectors are native borrows retained until drained replacement.
        ::uwvm2::uwvm::wasm::storage::details::selected_execute_wasm =
            ::std::addressof(candidate->file_for_native_initialization());
        ::uwvm2::uwvm::wasm::storage::details::selected_preloaded_wasm =
            ::std::addressof(candidate->preloaded_files_for_native_initialization());
        ::uwvm2::uwvm::runtime::storage::details::selected_runtime_registry =
            ::std::addressof(candidate->registry_for_native_initialization());
        return true;
    }

    inline void retire_selected_full_source_after_drain() noexcept
    {
        if(!details::selected_full_source) { return; }
        // Actual engines/readers/workers have already drained. Drop every loader
        // view before releasing the last selected source pin. Prepared debugger
        // tokens may independently retain their old source; epoch checks forbid
        // publishing that token into a replacement runtime generation.
        ::uwvm2::uwvm::wasm::storage::all_module_export.clear();
        ::uwvm2::uwvm::wasm::storage::all_module.clear();
        // [old strong selected source, all dependent views already erased]
        // [safe] no reader remains; reset borrowed selectors before ownership drops.
        ::uwvm2::uwvm::wasm::storage::details::selected_preloaded_wasm = nullptr;
        ::uwvm2::uwvm::wasm::storage::details::selected_execute_wasm = nullptr;
        ::uwvm2::uwvm::wasm::storage::details::selected_execute_wasm_initialized = false;
        ::uwvm2::uwvm::runtime::storage::details::selected_runtime_registry = nullptr;
        details::selected_full_source.reset();
    }
}


#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

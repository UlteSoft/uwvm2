#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <string>
# include <utility>
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/control/owned_file_image.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/validation/standard/wasm3/impl.h>
# include <uwvm2/uwvm/wasm/loader/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::checked_source
{
    // Actual SOURCE identity DATA only. Its SHA covers the complete SAME
    // immutable Wasm input, including custom sections and segment payloads.
    // Paths, names, version strings and a generated-object cache fingerprint
    // cannot substitute for these bytes. This carries no build/cache identity,
    // effective hot-replacement closure or checkpoint/restore authorization.
    struct parsed_source_module_identity
    {
        ::std::array<::std::byte, 32uz> source_sha256{};
        ::std::size_t source_byte_length{};
        ::std::size_t source_ordinal{};
        ::std::size_t parsed_import_count{};
        ::std::size_t local_function_count{};
        ::std::uint_least32_t binfmt_version{};
    };
    struct parsed_source_image_status
    {
        ::uwvm2::utils::control::owned_image_error error{};
        ::fast_io::error native_error{};
    };
    // Owned native compiler DATA, not an execution admission, resource handle,
    // generation lease or complete lowering. No caller-provided ready bit can
    // construct this type. The private factory owns parsing and the ONLY body
    // typing pass before module linking/initialization has any Wasm store effect.
    // This first closure covers one actual owned parsed file; multi-file loading,
    // dependency/provider setup and cross-source instantiation remain explicit work.
    // Configuration, file selection and reset are externally serialized and drained.
    // After the private typing seal, native callers must not reload/mutate this
    // file, its parsed metadata or loader configuration until drained retirement.
    // Existing global native borrows are read-only initializer inputs in this
    // staged path, never a guest-facing mutable source or an admission credential.
    class parsed_integer_source_plan final
    {
        using full_source = ::uwvm2::uwvm::runtime::full::full_source_instance;
        using function_plan = ::uwvm2::validation::standard::wasm3::retained_integer_function_plan;
        // The only mutable source owner remains private. Native initializers
        // borrow its preselected registry/file internally; this API exposes only
        // a const canonical owner after the typed parsing seal has succeeded.
        full_source::mutable_owner source_{};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t features_{};
        ::uwvm2::utils::container::vector<function_plan::owner> functions_{};
        struct declaration_identity { void const* code{}; void const* signature{}; };
        ::uwvm2::utils::container::vector<declaration_identity> declarations_{};
        ::std::uint_least64_t runtime_generation_{};
        ::std::array<parsed_source_module_identity, 1uz> source_modules_{};
        bool source_file_closure_complete_{};
        ::uwvm2::validation::standard::wasm3::retained_module_record_budget record_budget_{};
        ::std::size_t validated_functions_{};
        bool complete_function_index_{true};
        bool retention_limit_hit_{};
        explicit parsed_integer_source_plan(full_source::mutable_owner source, ::std::uint_least64_t generation,
            ::uwvm2::validation::standard::wasm3::retained_plan_limits limits,
            parsed_source_module_identity identity)
            : source_{::std::move(source)}, features_{source_->file().wasm_parameter.binfmt1_para}, runtime_generation_{generation},
              source_modules_{identity}, record_budget_{limits} {}
        template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        [[nodiscard]] static ::std::shared_ptr<parsed_integer_source_plan const> finish_typed_seal(
            ::std::unique_ptr<parsed_integer_source_plan> pending,
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& parsed,
            ::uwvm2::validation::error::code_validation_error_impl& error)
        {
            namespace validation = ::uwvm2::validation::standard::wasm3;
            auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(parsed.sections)};
            constexpr ::std::size_t function_import_bucket{};
            static_assert(function_import_bucket < imports.importdesc_count);
            // [actual owned parser import buckets] end
            // ^^ bucket 0 is compile-time in range before the metadata borrow.
            auto const imported{imports.importdesc.index_unchecked(function_import_bucket).size()};
            auto const& functions{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(parsed.sections).funcs};
            auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(parsed.sections).codes};
            auto const& types{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(parsed.sections).types};
            if(functions.size() != codes.size()) { return {}; }
            // Actual parser metadata, not caller-provided module/closure names.
            // This staged factory owns one file. Any import leaves its provider
            // source closure unresolved; no imported identity is guessed.
            pending->source_modules_[0uz].parsed_import_count = imports.imports.size();
            pending->source_modules_[0uz].local_function_count = codes.size();
            pending->source_file_closure_complete_ = imports.imports.empty();
            for(::std::size_t index{}; index != codes.size(); ++index)
            {
                auto const type_index{functions.index_unchecked(index)};
                if(type_index >= types.size() || index > (::std::numeric_limits<::std::size_t>::max)() - imported) { return {}; }
                // [codes/functions: index<size][types: type_index<size]
                // [owned parser metadata       ] no Wasm byte pointer is moved.
                // Fixed record ownership is bounded too: arbitrarily many
                // tiny functions cannot accumulate an unbounded owner vector.
                constexpr ::std::size_t entry_bytes{sizeof(function_plan) +
                    2uz * sizeof(function_plan::owner) + 2uz * sizeof(declaration_identity) + 4uz * sizeof(void*)};
                bool const retain_entry{pending->complete_function_index_ &&
                    pending->record_budget_.claim_fixed_metadata(entry_bytes)};
                auto checked{validation::details::retained_integer_plan_builder::validate(parsed,
                    imported + index, error, pending->features_, ::std::addressof(pending->record_budget_), retain_entry)};
                if(!checked || !checked->validation_sealed()) { return {}; }
                ++pending->validated_functions_;
                if(checked->unavailability() != validation::retained_plan_unavailability::none &&
                   checked->unavailability() != validation::retained_plan_unavailability::unsupported_instruction)
                { pending->retention_limit_hit_ = true; }
                if(retain_entry)
                {
                    pending->declarations_.push_back({::std::addressof(codes.index_unchecked(index)),
                        ::std::addressof(types.index_unchecked(type_index))});
                    pending->functions_.push_back(::std::move(checked));
                }
                else { pending->complete_function_index_ = false; pending->retention_limit_hit_ = true; }
            }
            // A valid unsupported lowering never exits early: every unused body
            // has completed the same Core3 typed decoder before this private mint.
            if(error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok ||
               pending->validated_functions_ != codes.size() || !pending->matches_selected_source() || pending->source_->initialized_from_actual_state() ||
               !pending->source_->registry().empty()) { return {}; }
            return ::std::shared_ptr<parsed_integer_source_plan const>{pending.release()};
        }
    public:
        using owner = ::std::shared_ptr<parsed_integer_source_plan const>;
        parsed_integer_source_plan(parsed_integer_source_plan const&) = delete;
        parsed_integer_source_plan& operator=(parsed_integer_source_plan const&) = delete;
        [[nodiscard]] static owner load_validate_after_drain(::std::u8string file_name, ::std::u8string rename,
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t const& features,
            ::uwvm2::validation::error::code_validation_error_impl& error,
            ::uwvm2::validation::standard::wasm3::retained_plan_limits limits = {},
            parsed_source_image_status* image_status = nullptr,
            ::std::size_t source_image_limit = ::uwvm2::utils::control::owned_file_image::default_bytes)
        {
            if(image_status != nullptr) { *image_status = {}; }
            namespace full = ::uwvm2::uwvm::runtime::full;
            namespace wasm = ::uwvm2::uwvm::wasm;
            namespace validation = ::uwvm2::validation::standard::wasm3;
            if(full::selected_full_source_owner_pin() || !wasm::storage::all_module.empty() ||
               !wasm::storage::all_module_export.empty() || !wasm::storage::preloaded_wasm.empty() ||
               !wasm::storage::preload_local_imported.empty() ||
               !::uwvm2::uwvm::runtime::storage::active_runtime_registry().empty() ||
               error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) { return {}; }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            if(!wasm::storage::preloaded_dl.empty()) { return {}; }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            if(!wasm::storage::weak_symbol.empty()) { return {}; }
#endif
            // Native file/read/quota/allocation failure is compiler resource
            // unavailability. It never writes a Wasm validation error, mints a
            // validity seal or falls back to a mutable mapped source.
            auto image{::uwvm2::utils::control::owned_file_image::read(file_name, source_image_limit)};
            if(!image)
            {
                if(image_status != nullptr) { *image_status = {image.error, image.native_error}; }
                return {};
            }
            auto const bytes{image.image->bytes()};
            // Actual private image factory proved nonempty allocated storage,
            // size<=PTRDIFF_MAX and source_image_limit BEFORE forming its end.
            // [bytes.data() ... bytes.data()+bytes.size()) one-past
            // [safe immutable actual input             ] ^^ digest end
            auto const digest_end{bytes.data() + bytes.size()};
            ::fast_io::sha256_context sha{}; sha.update(bytes.data(), digest_end); sha.do_final();
            parsed_source_module_identity identity{};
            // [identity.source_sha256: exactly32 writable bytes] end
            // [safe fixed digest destination                   ] no pointer advance
            sha.digest_to_byte_ptr(identity.source_sha256.data());
            identity.source_byte_length = bytes.size(); identity.source_ordinal = 0uz;
            auto const generation{::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api()};
            auto source{full_source::create_unparsed(::std::move(file_name), ::std::move(rename))};
            if(!full::select_unparsed_full_source_after_drain(source)) { return {}; }
            wasm::type::wasm_parameter_t parameters{}; parameters.binfmt1_para = features;
            if(wasm::loader::load_wasm_file(source->file_for_native_initialization(), source->owned_file_name(),
                source->owned_rename(), parameters, ::std::move(image.image)) != wasm::loader::load_wasm_file_rtl::ok) { return {}; }
            // The real file has adopted the SAME exclusive image before parsing.
            // bytes/digest_end are const borrows of that now-file-owned allocation;
            // successful adoption preserves both endpoints, never a postparse copy.
            // No construct_all_module, provider initialization, initialize_runtime,
            // const-expression execution, resource allocation, segment application
            // or start dispatch is permitted before ALL function seals succeed.
            if(source->file().binfmt_ver != 1u || !source->file().has_owned_source_image() ||
               source->file().source_size() != identity.source_byte_length ||
               source->file().source_cbegin() != reinterpret_cast<char const*>(bytes.data()) ||
               source->file().source_cend() != reinterpret_cast<char const*>(digest_end) ||
               source->initialized_from_actual_state() || !source->registry().empty()) { return {}; }
            identity.binfmt_version = source->file().binfmt_ver;
            auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(features)};
            if(!validation::uses_core3_validation_policy(policy)) { return {}; }
            ::std::unique_ptr<parsed_integer_source_plan> pending{new parsed_integer_source_plan{::std::move(source), generation, limits, identity}};
            auto const& parsed{pending->source_->file().wasm_module_storage.wasm_binfmt_ver1_storage};
            validation::validate_module_declarations_with_runtime_policy(parsed, error, pending->features_);
            return finish_typed_seal(::std::move(pending), parsed, error);
        }
        [[nodiscard]] full_source::owner source() const noexcept { return source_; }
        [[nodiscard]] auto const& features() const noexcept { return features_; }
        [[nodiscard]] ::std::span<parsed_source_module_identity const> source_module_identities() const noexcept
        {
            // [fixed actual source_modules_[0..1)] end
            // [safe const owned metadata         ] no caller-supplied order/span
            return {source_modules_.data(), source_modules_.size()};
        }
        // Source-file closure only; complete=false when any provider identity
        // is unresolved. Even true is not an effective-code/build/cache seal.
        [[nodiscard]] bool source_file_closure_complete() const noexcept { return source_file_closure_complete_; }
        [[nodiscard]] bool matches_selected_source() const noexcept
        {
            auto const selected{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            return source_ && selected && source_.get() == selected.get() &&
                !source_.owner_before(selected) && !selected.owner_before(source_) &&
                ::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api() == runtime_generation_;
        }
        [[nodiscard]] bool seal_actual_initializer() const noexcept
        {
            if(!matches_selected_source()) { return false; }
            if(source_->initialized_from_actual_state()) { return true; }
            // The unchanged source method verifies real registry membership,
            // actual published initializer serial and complete module phase;
            // this call accepts no guessed module pointer, ID, serial or ready bit.
            return source_->seal_actual_initializer();
        }
        [[nodiscard]] ::std::size_t function_count() const noexcept { return validated_functions_; }
        [[nodiscard]] bool complete_function_record_index() const noexcept { return complete_function_index_; }
        [[nodiscard]] bool retention_limit_hit() const noexcept { return retention_limit_hit_; }
        [[nodiscard]] ::std::size_t retained_record_operations() const noexcept { return record_budget_.operations; }
        [[nodiscard]] ::std::size_t retained_record_metadata_bytes() const noexcept { return record_budget_.metadata_bytes; }
        [[nodiscard]] function_plan::owner function(::std::size_t index) const noexcept
        { return index < functions_.size() ? functions_.index_unchecked(index) : function_plan::owner{}; }
        [[nodiscard]] bool matches_actual_function(::std::size_t index, void const* code, void const* signature) const noexcept
        {
            if(!matches_selected_source() || index >= declarations_.size()) { return false; }
            auto const& original{declarations_.index_unchecked(index)};
            return original.code == code && original.signature == signature;
        }
    };
}

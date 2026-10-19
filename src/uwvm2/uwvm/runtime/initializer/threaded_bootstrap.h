// Cold real preload-shape check. It performs no instantiation, writes or calls.
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <optional>
# include <utility>
# include "threaded_child_world.h"
#endif

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && !defined(UWVM_MODULE)
namespace uwvm2::uwvm::runtime::initializer
{
    enum class threaded_provider_error : unsigned char
    { ok, foreign_source, not_preload, invalid_file, effectful_provider, invalid_export, unsupported_memory, memory_limit };

    // Owns the actual canonical source and its actual preload leaf. No public
    // constructor can turn a pointer/boolean/name into this source-bound value.
    // This is not execution/spawn authority: Root must still admit the profile.
    class ThreadedMemoryProvider final
    {
    public:
        using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
        using module_type = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using file_type = ::uwvm2::uwvm::wasm::type::wasm_file_t;
        using memory_type = ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t;
    private:
        source_type::owner source_;
        module_type const* module_{};
        file_type const* file_{};
        memory_type const* memory_{};
        ::std::size_t module_id_{};
        ThreadedMemoryProvider(source_type::owner source, module_type const& module,
            file_type const& file, memory_type const& memory, ::std::size_t module_id) noexcept
            : source_{::std::move(source)}, module_{&module}, file_{&file}, memory_{&memory}, module_id_{module_id} {}
        template<typename Memory> [[nodiscard]] static bool stable_shared(Memory const& memory) noexcept
        {
            if constexpr(Memory::can_mmap && Memory::support_multi_thread && requires {
                memory.sequentially_consistent_size; memory.memory_length_p; memory.growing_mutex_p; })
            {
                return memory.custom_page_size_log2 == 16u && memory.sequentially_consistent_size &&
                    memory.memory_length_p != nullptr && memory.growing_mutex_p != nullptr;
            }
            else { return false; }
        }
    public:
        ThreadedMemoryProvider(ThreadedMemoryProvider const&) noexcept = default;
        ThreadedMemoryProvider(ThreadedMemoryProvider&&) noexcept = default;
        ThreadedMemoryProvider& operator=(ThreadedMemoryProvider const&) noexcept = default;
        ThreadedMemoryProvider& operator=(ThreadedMemoryProvider&&) noexcept = default;
        [[nodiscard]] auto const& source_owner() const noexcept { return source_; }
        [[nodiscard]] auto const& module() const noexcept { return *module_; }
        [[nodiscard]] auto const& file() const noexcept { return *file_; }
        [[nodiscard]] auto const& memory() const noexcept { return *memory_; }
        [[nodiscard]] ::std::size_t module_id() const noexcept { return module_id_; }
        [[nodiscard]] bool still_bound() const noexcept
        {
            return source_type::has_canonical_owner(source_) &&
                source_->bound_initialized_module_id(module_) == module_id_ &&
                source_->bound_initialized_file(module_id_, module_) == file_ &&
                module_->local_defined_memory_vec_storage.size() == 1uz &&
                &module_->local_defined_memory_vec_storage.index_unchecked(0uz) == memory_;
        }
        [[nodiscard]] static ::std::optional<ThreadedMemoryProvider> inspect(source_type::owner const& source,
            module_type const* actual, ::std::size_t maximum_shared_bytes, threaded_provider_error& error) noexcept
        {
            error = threaded_provider_error::foreign_source;
            if(!source_type::has_canonical_owner(source) || actual == nullptr) { return {}; }
            // Authenticate identity BEFORE dereferencing a supplied native pointer.
            bool member{};
            for(auto const& [name, module] : source->registry())
            { static_cast<void>(name); if(&module == actual) { member = true; break; } }
            if(!member) { return {}; }
            error = threaded_provider_error::not_preload;
            if(source->initialized_main_module() == actual) { return {}; }
            auto const id{source->bound_initialized_module_id(actual)};
            if(id == (::std::numeric_limits<::std::size_t>::max)()) { return {}; }
            auto const* file{source->bound_initialized_file(id, actual)};
            error = threaded_provider_error::invalid_file;
            if(file == nullptr || file == &source->file() || file->binfmt_ver != 1u || !file->has_owned_source_image()) { return {}; }
            auto const& m{*actual};
            error = threaded_provider_error::effectful_provider;
            if(!m.imported_function_vec_storage.empty() || !m.imported_memory_vec_storage.empty() ||
               !m.imported_global_vec_storage.empty() || !m.imported_table_vec_storage.empty() ||
               !m.imported_tag_vec_storage.empty() || !m.local_defined_function_vec_storage.empty() ||
               !m.local_defined_code_vec_storage.empty() || !m.local_defined_table_vec_storage.empty() ||
               !m.local_defined_global_vec_storage.empty() || !m.local_defined_tag_vec_storage.empty() ||
               !m.local_defined_element_vec_storage.empty() || !m.local_defined_data_vec_storage.empty() ||
               m.type_section_storage.type_section_count != 0uz) { return {}; }
            using start_section = ::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t;
            auto const& start{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section>(
                file->wasm_module_storage.wasm_binfmt_ver1_storage.sections)};
            if(start.sec_span.sec_begin != nullptr) { return {}; }
            error = threaded_provider_error::unsupported_memory;
            if(m.local_defined_memory_vec_storage.size() != 1uz) { return {}; }
            auto const& leaf{m.local_defined_memory_vec_storage.index_unchecked(0uz)};
            auto const* type{leaf.memory_type_ptr};
            if(type == nullptr || !details::wasm_memory_is_shared(*type) || details::wasm_memory_is_address64(*type) ||
               !type->limits.present_max || !leaf.effective_limits.present_max || !stable_shared(leaf.memory)) { return {}; }
            if(type->limits.min != leaf.effective_limits.min || type->limits.max != leaf.effective_limits.max) { return {}; }
            error = threaded_provider_error::memory_limit;
            auto const maximum{static_cast<::std::uint_least64_t>(leaf.effective_limits.max)};
            if(maximum_shared_bytes == 0uz || maximum > maximum_shared_bytes / 65536u) { return {}; }
            auto const bytes{leaf.memory.memory_length_p->load(::std::memory_order_seq_cst)};
            if(bytes > maximum * 65536u || bytes < static_cast<::std::uint_least64_t>(type->limits.min) * 65536u) { return {}; }
            // Read actual parser declarations, including exports; runtime count
            // metadata alone cannot admit an unexamined effectful module.
            bool const declarations{[&]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(
                ::uwvm2::utils::container::tuple<Fs...>) noexcept
            {
                using memory_section = ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>;
                using export_section = ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>;
                auto const& sections{file->wasm_module_storage.wasm_binfmt_ver1_storage.sections};
                // Strict first bootstrap: only memory/export/custom sections.
                // Even empty declaration/effect sections are not admitted here.
                namespace w1 = ::uwvm2::parser::wasm::standard::wasm1::features;
                namespace w11 = ::uwvm2::parser::wasm::standard::wasm1p1::features;
                auto absent{[&]<typename Section>() noexcept
                { return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<Section>(sections).sec_span.sec_begin == nullptr; }};
                if(!absent.template operator()<w1::type_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w1::import_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w1::function_section_storage_t>() ||
                   !absent.template operator()<w1::table_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w1::global_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w1::element_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w1::code_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w1::data_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w11::tag_section_storage_t<Fs...>>() ||
                   !absent.template operator()<w11::data_count_section_storage_t<Fs...>>()) { return false; }
                auto const& memories{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<memory_section>(sections)};
                if(memories.memories.size() != 1uz || &memories.memories.index_unchecked(0uz) != type) { return false; }
                auto const& exports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<export_section>(sections)};
                if(exports.exports.empty()) { return false; }
                for(auto const& item : exports.exports)
                {
                    if(item.exports.type != ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory ||
                       item.exports.storage.memory_idx != 0u) { return false; }
                }
                return true;
            }(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};
            error = threaded_provider_error::invalid_export;
            if(!declarations) { return {}; }
            error = threaded_provider_error::ok;
            return ThreadedMemoryProvider{source, m, *file, leaf, id};
        }
    };
}
#endif

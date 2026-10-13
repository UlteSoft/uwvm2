// Private cold resource input producer. Proposed include inside the actual
// runtime namespace after full publications/real execution and publication
// guards. Not selected by the current runtime; no ordinary guest IR is emitted.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++"
{
    class runtime_checkpoint_coherent_manager;
    class runtime_checkpoint_resource_inputs final
    {
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
        using lifetime_lease = ::uwvm2::utils::thread::execution_domain::lease;
        using closed_admission = ::uwvm2::utils::thread::checkpoint_host_admission::closed_admission;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
        using profile_owner = ::uwvm2::runtime::checkpoint::compilation_profile::owner;
        using file_type = ::uwvm2::uwvm::wasm::type::wasm_file_t;
        using module_type = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    public:
        enum class status : unsigned char
        {
            immutable_inputs_only, invalid_management_scope, invalid_cohort,
            invalid_publication, unsupported_source_origin, unsupported_registry,
            unknown_import_census, stale_function_generation, invalid_segment_origin,
            quota_exceeded, allocation_failed
        };
        struct data_segment
        {
            ::std::uint64_t index{}, source_offset{}, byte_count{};
            bool dropped{};
        };
        struct module_data
        {
            ::std::uint64_t module{};
            // Declaration counts in exact function/table/memory/global/tag/data/
            // element index order. No native vector, pointer or JIT view is copied.
            ::std::array<::std::uint64_t, 7u> declaration_counts{};
            ::std::vector<::std::byte> original_module{};
            ::std::vector<data_segment> data{};
        };
        struct result
        {
            status observation{status::invalid_management_scope};
            ::std::uint64_t observed_runtime_epoch{};
            ::std::vector<module_data> modules{};
            // This particular DATA producer has no whole mutable-resource, root,
            // host adapter, publication, restore or replay credential.
            inline static constexpr bool complete_instance = false;
            inline static constexpr bool executable_restore_available = false;
        };
    private:
        static_assert(sizeof(::std::size_t) <= sizeof(::std::uint64_t));
        static constexpr ::std::size_t module_limit{4096u}, segment_limit{1048576u}, thread_limit{256u};
        static constexpr ::std::size_t payload_limit{256u * 1024u * 1024u};
        lifetime_lease const& lease_;
        runtime_state_publication_guard const& publication_;
        closed_admission const& closed_;
        ::std::span<domain::stopped_participant const> const cohort_;
        profile_owner const& profile_;
        ::std::uint_least64_t const epoch_;

        // SOLE issuer: the named manager, synchronously INSIDE its genuine ONE
        // current-ticket/cooperatively-stopped cohort callback, AFTER all actual
        // capture-owner/episode/frame checks and the real nonwaiting host close
        // and publication guard. No public IDs/report/closed-count or caller bool
        // can construct this object. Do not create it outside that lexical scope.
        runtime_checkpoint_resource_inputs(lifetime_lease const& lease,
            runtime_state_publication_guard const& publication, closed_admission const& closed,
            ::std::span<domain::stopped_participant const> cohort, profile_owner const& profile,
            ::std::uint_least64_t epoch) noexcept
            : lease_{lease}, publication_{publication}, closed_{closed}, cohort_{cohort}, profile_{profile}, epoch_{epoch} {}
        runtime_checkpoint_resource_inputs(runtime_checkpoint_resource_inputs const&) = delete;
        runtime_checkpoint_resource_inputs& operator=(runtime_checkpoint_resource_inputs const&) = delete;
        runtime_checkpoint_resource_inputs(runtime_checkpoint_resource_inputs&&) = delete;
        runtime_checkpoint_resource_inputs& operator=(runtime_checkpoint_resource_inputs&&) = delete;
        ~runtime_checkpoint_resource_inputs() = default;

        template<typename Owner>
        [[nodiscard]] static bool same_owner(Owner const& a, Owner const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
        [[nodiscard]] bool current_scope() const noexcept
        {
            // publication_ is a retained REFERENCE to the actual lexical guard,
            // not a numeric assertion or an independently acquired nested guard.
            (void)publication_;
            return lease_ && !lease_.stop_requested() && closed_ && epoch_ != 0u &&
                current_runtime_generation() == epoch_ && get_runtime_state_publication_depth() == 1u &&
                !cohort_.empty() && cohort_.size() <= thread_limit &&
                same_owner(profile_, g_runtime.checkpoint_profile) &&
                g_runtime.compiled_all.load(::std::memory_order_acquire);
        }
        [[nodiscard]] static bool span_offset(file_type const& file, void const* begin, void const* end,
            ::std::size_t& offset, ::std::size_t& length) noexcept
        {
            if(!file.has_owned_source_image() || file.source_size() < 8u ||
               file.source_cbegin() == nullptr || begin == nullptr || end == nullptr) { return false; }
            auto const base{reinterpret_cast<::std::uintptr_t>(file.source_cbegin())};
            auto const first{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const last{reinterpret_cast<::std::uintptr_t>(end)};
            // These genuine runtime/parser endpoints are compared only. No
            // arbitrary begin/end address is ever dereferenced or advanced.
            if(file.source_size() > UINTPTR_MAX - base || first < base || last < first ||
               last - base > file.source_size()) { return false; }
            offset = static_cast<::std::size_t>(first - base);
            length = static_cast<::std::size_t>(last - first);
            return offset <= file.source_size() && length <= file.source_size() - offset;
        }
        template<::uwvm2::parser::wasm::concepts::wasm_feature... Features>
        [[nodiscard]] static status check_data_origins(file_type const& file, module_type const& module,
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Features...> const& parsed) noexcept
        {
            using section = ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Features...>;
            auto const& declarations{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<section>(parsed.sections)};
            if(declarations.datas.size() != module.local_defined_data_vec_storage.size())
            { return status::invalid_segment_origin; }
            for(::std::size_t i{}; i != declarations.datas.size(); ++i)
            {
                // [actual owned parser and module data arrays, 0<=i<N] end
                // [safe] equal current lengths checked BEFORE indexing; the
                // runtime declaration token is COMPARED before any pointee read.
                auto const& declared{declarations.datas.index_unchecked(i)};
                auto const& runtime{module.local_defined_data_vec_storage.index_unchecked(i)};
                if(runtime.data_type_ptr != ::std::addressof(declared) ||
                   runtime.data.byte_begin != reinterpret_cast<::std::byte const*>(declared.storage.segment.byte.begin) ||
                   runtime.data.byte_end != reinterpret_cast<::std::byte const*>(declared.storage.segment.byte.end))
                { return status::invalid_segment_origin; }
                ::std::size_t offset{}, length{};
                if(!span_offset(file, runtime.data.byte_begin, runtime.data.byte_end, offset, length))
                { return status::invalid_segment_origin; }
            }
            return status::immutable_inputs_only;
        }
        [[nodiscard]] status check_module(::std::size_t id, source_owner& source) const noexcept
        {
            if(id >= g_runtime.modules.size()) { return status::invalid_publication; }
            // [actual current dense module records, id<N] end
            // [safe] actual lease+ONE domain+publication, no caller module pointer.
            auto const& record{g_runtime.modules.index_unchecked(id)};
            auto const* code{record.llvm_jit_full_publication.get()};
            if(code == nullptr || record.runtime_module == nullptr || !record.llvm_jit_ready ||
               !code->engine || !code->context || code->plan || code->debug_source_runtime_epoch != epoch_ ||
               record.llvm_jit_debug_source_fused_epoch != epoch_ || !same_owner(code->checkpoint_profile, profile_))
            { return status::invalid_publication; }
            source = code->source; // Actual publication owns this canonical pin.
            if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
               !source->initialized_from_actual_state() || source->actual_full_validation_epoch() != epoch_ ||
               source->bound_initialized_main_module_id() != id || source->registry().size() != 1u)
            { return status::unsupported_registry; }
            auto const member{source->registry().find(record.module_name)};
            if(member == source->registry().end() || ::std::addressof(member->second) != record.runtime_module ||
               source->initialized_main_module() != record.runtime_module)
            { return status::invalid_publication; }
            // [genuine initialized member of THIS actual canonical registry]
            // [safe] raw runtime_module was resolved to this member BEFORE read.
            auto const& module{member->second};
            auto const& file{source->file()};
            if(file.binfmt_ver != 1u || !file.has_owned_source_image())
            { return status::unsupported_source_origin; }
            // First source-input slice rejects all external index-space bindings.
            // It does not infer provider purity, escaped-view absence, a resource
            // serializer or deterministic effects from a host-op count of zero.
            if(!module.imported_function_vec_storage.empty() || !module.imported_memory_vec_storage.empty() ||
               !module.imported_table_vec_storage.empty() || !module.imported_global_vec_storage.empty() ||
               !module.imported_tag_vec_storage.empty()) { return status::unknown_import_census; }
            auto const functions{module.local_defined_function_vec_storage.size()};
            if(record.llvm_jit_debug_full_entry_generations.size() != functions ||
               record.llvm_jit_compiled.local_funcs.size() != functions)
            { return status::stale_function_generation; }
            for(auto generation : record.llvm_jit_debug_full_entry_generations)
            { if(generation != 1u) { return status::stale_function_generation; } }
            return check_data_origins(file, module, file.wasm_module_storage.wasm_binfmt_ver1_storage);
        }
        [[nodiscard]] result copy_immutable_inputs() const noexcept
        {
            result out{};
            if(!current_scope()) { return out; }
            auto const modules{g_runtime.modules.size()};
            if(modules == 0u || modules > module_limit)
            { out.observation = status::quota_exceeded; return out; }
            try
            {
                // Complete actual dense cohort, including modules absent from all
                // live frames. ALL origins/counts/quotas are checked before byte
                // copying, allocation of payload vectors or positive DATA output.
                ::std::vector<source_owner> sources(modules);
                ::std::size_t source_bytes{}, segments{};
                for(::std::size_t id{}; id != modules; ++id)
                {
                    auto const checked{check_module(id, sources[id])};
                    if(checked != status::immutable_inputs_only) { out.observation = checked; return out; }
                    auto const& file{sources[id]->file()};
                    auto const count{sources[id]->initialized_main_module()->local_defined_data_vec_storage.size()};
                    if(file.source_size() > payload_limit - source_bytes || count > segment_limit - segments)
                    { out.observation = status::quota_exceeded; return out; }
                    source_bytes += file.source_size(); segments += count; // Checked BEFORE additions.
                }
                if(!current_scope()) { return out; }
                result candidate{}; candidate.modules.reserve(modules);
                for(::std::size_t id{}; id != modules; ++id)
                {
                    // [preflight-qualified owned source vector, id<modules<=4096]
                    // [safe] source/file/module identity is privately held by the
                    // same lexical publication, lease, closed gate and ONE cohort.
                    auto const& source{sources[id]}; auto const& file{source->file()};
                    auto const& module{*source->initialized_main_module()};
                    module_data data{}; data.module = id;
                    data.declaration_counts = {module.local_defined_function_vec_storage.size(),
                        module.local_defined_table_vec_storage.size(), module.local_defined_memory_vec_storage.size(),
                        module.local_defined_global_vec_storage.size(), module.local_defined_tag_vec_storage.size(),
                        module.local_defined_data_vec_storage.size(), module.local_defined_element_vec_storage.size()};
                    auto const size{file.source_size()};
                    if(size > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
                       size > data.original_module.max_size()) { out.observation = status::quota_exceeded; return out; }
                    data.original_module.resize(size);
                    auto const* bytes{reinterpret_cast<::std::byte const*>(file.source_cbegin())};
                    // [actual exclusively owned immutable input bytes 0..size] end
                    // [safe] size<=preflight payload quota/vector/PTRDIFF bound;
                    // copying from its SAME canonical base, never endpoint tokens.
                    // Original host-file truncation cannot invalidate this owner.
                    ::fast_io::freestanding::my_memcpy(data.original_module.data(), bytes, size);
                    data.data.reserve(module.local_defined_data_vec_storage.size());
                    for(::std::size_t i{}; i != module.local_defined_data_vec_storage.size(); ++i)
                    {
                        auto const& segment{module.local_defined_data_vec_storage.index_unchecked(i).data};
                        ::std::size_t offset{}, length{};
                        if(!span_offset(file, segment.byte_begin, segment.byte_end, offset, length))
                        { out.observation = status::invalid_segment_origin; return out; }
                        bool const dropped{::uwvm2::uwvm::runtime::storage::wasm_data_segment_is_dropped(segment)};
                        // A dropped segment has zero logical payload. Retaining
                        // exact original module bytes is still needed for typing,
                        // declarations/custom info and any future detached restore.
                        data.data.push_back({i, dropped ? 0u : offset, dropped ? 0u : length, dropped});
                    }
                    candidate.modules.push_back(::std::move(data));
                }
                if(!current_scope()) { return out; }
                candidate.observed_runtime_epoch = epoch_;
                candidate.observation = status::immutable_inputs_only;
                return candidate; // Detached owned DATA; no lexical borrow escapes.
            }
            catch(...) { out.observation = status::allocation_failed; return out; }
        }
    };
}
#endif

// Private native restore preparation; ordinary initialize_runtime is unchanged.
#pragma once
namespace uwvm2::runtime::lib
{
    extern "C++" { class runtime_checkpoint_world_transaction; }
}
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer
{
    extern "C++"
    {
        class restoration_context;
        // Owned compilation DATA only. This native record cannot issue an epoch,
        // execution lease, GC publication, cache admission or restore credential.
        class staged_compiler_module_owner final
        {
            friend class restoration_context;
            using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
            using storage_module = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
            using file_type = ::uwvm2::uwvm::wasm::type::wasm_file_t;
            source_type::owner source_{};
            storage_module const* module_{};
            file_type const* file_{};
            explicit staged_compiler_module_owner(source_type::owner source,
                storage_module const* module, file_type const* file) noexcept
                : source_{::std::move(source)}, module_{module}, file_{file} {}
        public:
            staged_compiler_module_owner() = delete;
            staged_compiler_module_owner(staged_compiler_module_owner const&) = default;
            staged_compiler_module_owner& operator=(staged_compiler_module_owner const&) = delete;
            staged_compiler_module_owner(staged_compiler_module_owner&&) = default;
            staged_compiler_module_owner& operator=(staged_compiler_module_owner&&) = delete;
            ~staged_compiler_module_owner() = default;
            [[nodiscard]] source_type::owner const& source() const noexcept { return source_; }
            [[nodiscard]] storage_module const* module() const noexcept { return module_; }
            [[nodiscard]] file_type const* file() const noexcept { return file_; }
            [[nodiscard]] bool matches_unpublished_file() const noexcept;
        };
        class restoration_context final
        {
            friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;
            friend class staged_compiler_module_owner;
            template<details::initialization_purpose> friend class details::initialization_context;
            using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
            using source_owner = source_type::mutable_owner;
            using storage_module = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
            using store_type = ::uwvm2::uwvm::runtime::storage::gc_object_store;
            using reference = ::uwvm2::uwvm::runtime::storage::gc_reference;
            using staged_gc = ::uwvm2::uwvm::runtime::storage::checkpoint_gc_staging;
            static constexpr auto purpose{details::initialization_purpose::unpublished_restore};
            enum class skeleton_result : unsigned { empty, built, invalid_source, duplicate_module, dependency_error };
            source_owner source_{};
            staged_gc& staged_gc_;
            ::uwvm2::utils::container::u8string_view current_module_{};
            bool alias_sanity_{};
            initializer_limit_t limits_{initializer_limit};
            details::initialization_context<purpose> world_;
            skeleton_result state_{skeleton_result::empty};

            explicit restoration_context(source_owner source, staged_gc& stage) noexcept
                : source_{::std::move(source)}, staged_gc_{stage},
                  world_{}
            {
                // Private transaction MUST supply a real canonical nonmoving source.
                // Do not dereference a caller/serialized source address as a credential.
                // Canonical lifetime and native source identity precede every field borrow;
                // the native consistency check never grants execution authority.
                if(!source_type::has_canonical_owner(source_) || source_->initialized_ ||
                    !source_->registry_.empty() || !source_->checkpoint_declarations_.empty() ||
                    !source_->checkpoint_exports_.empty() || !source_->checkpoint_import_resets_.empty() ||
                    !source_->checkpoint_memory_limits_.empty())
                { state_ = skeleton_result::invalid_source; return; }
                world_.registry_ = ::std::addressof(source_->registry_);
                world_.declarations_ = ::std::addressof(source_->checkpoint_declarations_);
                world_.exports_ = ::std::addressof(source_->checkpoint_exports_);
                world_.resets_ = ::std::addressof(source_->checkpoint_import_resets_);
                world_.memory_limits_ = ::std::addressof(source_->checkpoint_memory_limits_);
                world_.current_ = ::std::addressof(current_module_); world_.sanity_ = ::std::addressof(alias_sanity_);
                world_.limits_ = ::std::addressof(limits_); world_.restoration_ = this;
            }
            enum class source_copy_status : unsigned char
            { prepared_unpublished, invalid_actual_source, source_limit, copy_failed, parser_failed };
            struct source_copy_result
            {
                source_owner source{};
                source_copy_status status{source_copy_status::invalid_actual_source};
            };
            // Private original-module COPY preparation only. The actual world
            // issuer separately compares the saved envelope/product/cache/source,
            // dense module correspondence and ALL saved effective generation bodies.
            // A parsed copy does not issue a compilation/epoch/restore credential.
            [[nodiscard]] static source_copy_result prepare_owned_original_source(
                source_type::owner const& actual, ::std::uint_least64_t observed_epoch,
                ::std::size_t maximum_total_bytes) noexcept
            {
                using image_type = ::uwvm2::utils::control::owned_file_image;
                using file_type = ::uwvm2::uwvm::wasm::type::wasm_file_t;
                if(!source_type::has_canonical_owner(actual) || observed_epoch == 0u ||
                    !actual->initialized_from_actual_state() || actual->preload_files_.size() > 4095u ||
                    actual->preload_files_.size() != actual->preload_inputs_.size() ||
                    actual->preload_files_.size() != actual->preload_bindings_.size() ||
                    actual->actual_validated_file(actual->main_module_id_, observed_epoch, actual->main_module_) !=
                        ::std::addressof(actual->file_)) { return {}; }
                // Real native maintenance+execution/source publication proof is
                // held by the private transaction throughout this synchronous work.
                // Membership and actual finalized validation epoch BEFORE pointer
                // or parser-byte borrows; supplied integers cannot mint that proof.
                ::std::size_t total{};
                auto valid_file = [&](file_type const& file) noexcept
                {
                    auto const count{file.source_size()};
                    if(file.binfmt_ver != 1u || !file.has_owned_source_image() || count < 8u ||
                        count > image_type::maximum_bytes || count > maximum_total_bytes - total ||
                        count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
                        file.source_cbegin() == nullptr || count > UINTPTR_MAX -
                            reinterpret_cast<::std::uintptr_t>(file.source_cbegin())) { return false; }
                    total += count; // checked remaining quota BEFORE addition.
                    return true;
                };
                if(maximum_total_bytes < 8u || !valid_file(actual->file_))
                { return {{}, source_copy_status::source_limit}; }
                for(::std::size_t index{}; index != actual->preload_files_.size(); ++index)
                {
                    // [FINAL owned bindings/files] one-past
                    // [safe] index < actual final extents BEFORE index_unchecked.
                    auto const& binding{actual->preload_bindings_[index]};
                    auto const& file{actual->preload_files_.index_unchecked(index)};
                    if(actual->actual_validated_file(binding.id, observed_epoch, binding.module) !=
                        ::std::addressof(file)) { return {}; }
                    if(!valid_file(file)) { return {{}, source_copy_status::source_limit}; }
                }
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    auto copy_image = [](file_type const& file) noexcept
                    {
                        // [actual immutable owned ORIGINAL module, exact count] end
                        // [safe] ALL module identity/extent/quota/uintptr checks
                        // above BEFORE forming native span/copy. No address is
                        // read from serialized DATA; factory copies into fresh
                        // exclusive storage and retains no old parser pointers.
                        return image_type::copy_owned_bytes({reinterpret_cast<::std::byte const*>(
                            file.source_cbegin()), file.source_size()}, image_type::maximum_bytes);
                    };
                    auto main_image{copy_image(actual->file_)};
                    if(!main_image) { return {{}, source_copy_status::copy_failed}; }
                    ::std::vector<::uwvm2::uwvm::runtime::full::full_preload_input> inputs{};
                    if(actual->preload_inputs_.size() > inputs.max_size() || actual->preload_inputs_.size() >
                        static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                            sizeof(::uwvm2::uwvm::runtime::full::full_preload_input))
                    { return {{}, source_copy_status::source_limit}; }
                    inputs.reserve(actual->preload_inputs_.size());
                    for(::std::size_t index{}; index != actual->preload_inputs_.size(); ++index)
                    {
                        // [actual FINAL inputs/files] one-past
                        // [safe] paired sizes proved BEFORE each index borrow.
                        auto const& original{actual->preload_inputs_[index]};
                        auto const& file{actual->preload_files_.index_unchecked(index)};
                        auto image{copy_image(file)};
                        if(!image) { return {{}, source_copy_status::copy_failed}; }
                        inputs.push_back({original.file_name, original.module_name,
                            file.wasm_parameter, ::std::move(image.image)});
                    }
                    // All terminated filenames, exact native rename policy and
                    // actual per-module syntax+parser quotas belong to their FINAL
                    // nonmoving source BEFORE any fresh parser borrow is created.
                    // This does not reopen files or follow a new filesystem name.
                    auto prepared{source_type::create_unparsed(actual->file_name_storage_,
                        actual->rename_storage_, ::std::move(inputs))};
                    using parsed = ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl;
                    if(::uwvm2::uwvm::wasm::loader::load_wasm_file(prepared->file_, prepared->owned_file_name(),
                        prepared->owned_rename(), actual->file_.wasm_parameter, ::std::move(main_image.image)) != parsed::ok ||
                        !prepared->file_.has_owned_source_image() ||
                        prepared->file_.module_name != actual->file_.module_name)
                    { return {{}, source_copy_status::parser_failed}; }
                    for(::std::size_t index{}; index != prepared->preload_files_.size(); ++index)
                    {
                        // [new FINAL parsed files, original FINAL files] end
                        // [safe] clone factory fixed SAME extent before this loop;
                        // index checked before both unchecked member borrows.
                        auto& file{prepared->preload_files_.index_unchecked(index)};
                        auto const& original{actual->preload_files_.index_unchecked(index)};
                        auto image{prepared->take_preloaded_image_for_native_initialization(index)};
                        if(::uwvm2::uwvm::wasm::loader::load_wasm_file(file, file.file_name, file.module_name,
                            file.wasm_parameter, ::std::move(image)) != parsed::ok || !file.has_owned_source_image() ||
                            file.module_name != original.module_name)
                        { return {{}, source_copy_status::parser_failed}; }
                    }
                    // Keep fresh initializer serial/phase/validation epoch ZERO.
                    // No source selector, native adapter init, code publication,
                    // active segment, original constant expression or start runs.
                    // The true joint fixup/compiler/drain/commit issuer is separate.
                    return {::std::move(prepared), source_copy_status::prepared_unpublished};
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::std::bad_alloc const&) { return {{}, source_copy_status::copy_failed}; }
#endif
            }
            [[nodiscard]] static ::uwvm2::uwvm::wasm::type::wasm_file_t const* actual_unpublished_file(
                source_type::owner const& source, storage_module const* candidate) noexcept
            {
                if(candidate == nullptr || !source_type::has_canonical_owner(source) || source->initialized_) { return nullptr; }
                for(auto const& [name, module] : source->registry_)
                {
                    // [actual FINAL owned registry] end
                    // [safe] candidate is only an address comparison BEFORE any
                    // supplied module pointee read. Actual loop element owns metadata.
                    if(::std::addressof(module) != candidate) { continue; }
                    auto const declaration{source->checkpoint_declarations_.find(name)};
                    using module_kind = ::uwvm2::uwvm::wasm::type::module_type_t;
                    if(declaration == source->checkpoint_declarations_.end() ||
                        (declaration->second.type != module_kind::exec_wasm &&
                         declaration->second.type != module_kind::preloaded_wasm)) { return nullptr; }
                    // Actual Wasm discriminator BEFORE the union's wf read.
                    auto const* file{declaration->second.module_storage_ptr.wf};
                    auto valid_file = [&](auto const& actual) noexcept
                    {
                        // File identity was proved by the loop/field address below
                        // BEFORE these immutable actual parser-owned field reads.
                        return actual.binfmt_ver == 1u && actual.has_owned_source_image() && actual.module_name == name;
                    };
                    if(file == ::std::addressof(source->file_))
                    { return valid_file(source->file_) ? file : nullptr; }
                    for(auto const& preload : source->preload_files_)
                    {
                        if(file == ::std::addressof(preload)) { return valid_file(preload) ? file : nullptr; }
                    }
                    return nullptr;
                }
                return nullptr;
            }
            [[nodiscard]] ::std::unique_ptr<staged_compiler_module_owner> prepare_compiler_module_owner(
                storage_module const* candidate)
            {
                if(state_ != skeleton_result::built) { return {}; }
                source_type::owner actual{source_};
                auto const* file{actual_unpublished_file(actual, candidate)};
                if(file == nullptr) { return {}; }
                // Private construction only after actual source/control-block,
                // module-vector, declaration discriminator and final owned WF
                // membership. Strong source owns all FINAL maps/parser/records.
                // No ordinary initialized/source-ready/epoch flag is written.
                return ::std::unique_ptr<staged_compiler_module_owner>{
                    new staged_compiler_module_owner{::std::move(actual), candidate, file}};
            }
            using saved_core_type = ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type;
            using saved_core_kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
            using native_value = ::uwvm2::uwvm::runtime::storage::gc_object_value;
            struct saved_memory_chunk
            { ::std::uint64_t offset{}; ::std::span<::std::byte const> bytes{}; };
            struct saved_table_chunk
            { ::std::uint64_t offset{}; ::std::span<reference const> values{}; };
            struct completion_record
            { ::std::uintptr_t address{}; bool completed{}; };
            // Fixed sorted actual native comparison keys, never authority or wire
            // pointers. Fill/final checks cost O(log R), not a growing O(R) scan.
            ::std::vector<completion_record> completed_resource_records_{};
            ::std::size_t expected_resource_records_{}, completed_resource_count_{}, function_import_hop_limit_{};
            bool resource_fixups_attempted_{}, resource_fixups_started_{};
            struct element_allocation
            {
                storage_module* module{};
                ::std::size_t function_count{}, opaque_count{}, gc_count{};
                ::std::size_t function_cursor{}, opaque_cursor{}, gc_cursor{};
            };
            ::std::vector<element_allocation> element_allocations_{};
            [[nodiscard]] bool begin_resource_fixups(::std::size_t max_records, ::std::size_t maximum_native_element_bytes)
            {
                if(state_ != skeleton_result::built || resource_fixups_attempted_ || !source_ || source_->initialized_ ||
                    !source_type::has_canonical_owner(source_)) { return false; }
                // One attempted build owns all partial private allocations. Any
                // failure is discard-only; it cannot append a second roster or
                // reuse partly grown element owners. No ordinary path observes it.
                resource_fixups_attempted_=true;
                ::std::size_t count{};
                for(auto const& [name, module] : source_->registry_)
                {
                    static_cast<void>(name);
                    for(auto extent : {module.local_defined_memory_vec_storage.size(), module.local_defined_table_vec_storage.size(),
                        module.local_defined_global_vec_storage.size(), module.local_defined_data_vec_storage.size(),
                        module.local_defined_element_vec_storage.size()})
                    {
                        if(count > max_records || extent > max_records-count) { return false; }
                        count += extent; // complete actual record quota BEFORE sum.
                    }
                }
                if(count > completed_resource_records_.max_size() || count >
                    static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(completion_record)) { return false; }
                // Build the FINAL roster before resource mutation. Each key comes
                // from a live context-owned vector element; no candidate is read.
                completed_resource_records_.reserve(count);
                for(auto const& [name, module] : source_->registry_)
                {
                    static_cast<void>(name);
                    auto append = [&](auto const& records) noexcept
                    {
                        for(auto const& actual : records)
                        { completed_resource_records_.push_back({reinterpret_cast<::std::uintptr_t>(::std::addressof(actual)),false}); }
                    };
                    append(module.local_defined_memory_vec_storage); append(module.local_defined_table_vec_storage);
                    append(module.local_defined_global_vec_storage); append(module.local_defined_data_vec_storage);
                    append(module.local_defined_element_vec_storage);
                    auto const imports{module.imported_function_vec_storage.size()};
                    if(imports > max_records-function_import_hop_limit_) { return false; }
                    function_import_hop_limit_ += imports; // checked total imported chain bound BEFORE sum.
                }
                if(completed_resource_records_.size() != count) { return false; }
                ::std::sort(completed_resource_records_.begin(),completed_resource_records_.end(),
                    [](auto const& a,auto const& b) noexcept { return a.address < b.address; });
                for(::std::size_t n{1u}; n < completed_resource_records_.size(); ++n)
                {
                    // [FINAL sorted actual element keys] end
                    // [safe] n>=1 and n<size BEFORE n-1/n comparisons.
                    if(completed_resource_records_[n-1u].address == completed_resource_records_[n].address) { return false; }
                }
                if(source_->registry_.size() > element_allocations_.max_size() || source_->registry_.size() >
                    static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(element_allocation)) { return false; }
                element_allocations_.reserve(source_->registry_.size());
                ::std::size_t bytes_used{};
                using family = ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family;
                for(auto& [name, module] : source_->registry_)
                {
                    static_cast<void>(name);
                    if(!module.element_expr_funcref_vec_storage.empty() || !module.element_expr_externref_vec_storage.empty() ||
                       !module.element_expr_gc_ref_vec_storage.empty() || unpublished_file_for_module(::std::addressof(module)) == nullptr)
                    { return false; }
                    element_allocation allocation{}; allocation.module = ::std::addressof(module);
                    for(auto const& record : module.local_defined_element_vec_storage)
                    {
                        if(record.element_type_ptr == nullptr) { return false; }
                        // [actual context-created record][FINAL owned parser declaration]
                        // [safe] the built private initializer installed immutable
                        // declarations from its owned file; no supplied pointer is read.
                        auto const& segment{record.element_type_ptr->storage.segment};
                        auto const count{segment.vec_expr.size()};
                        if(count == 0u) { continue; }
                        if(!segment.vec_funcidx.empty()) { return false; }
                        auto const kind{::uwvm2::uwvm::runtime::storage::runtime_element_family(record, module)};
                        auto& total{kind == family::function ? allocation.function_count :
                            (kind == family::external || kind == family::exception ? allocation.opaque_count : allocation.gc_count)};
                        auto const width{kind == family::function ? sizeof(::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t) :
                            (kind == family::external || kind == family::exception ? sizeof(void*) : sizeof(reference))};
                        if(total > SIZE_MAX-count || bytes_used > maximum_native_element_bytes ||
                           count > (maximum_native_element_bytes-bytes_used)/width ||
                           count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/width)
                        { return false; }
                        auto const bound{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/width};
                        if(total > bound || count > bound-total) { return false; }
                        total += count; bytes_used += count*width; // all remaining quotients checked BEFORE sum/product.
                    }
                    element_allocations_.push_back(allocation); // registry-size reserve bounds this append.
                }
                // ALL shape/count/native byte extents are proved BEFORE allocating
                // final passive element owners. They NEVER grow after any range is
                // installed, so later segment fill cannot relocate an earlier range.
                for(auto const& allocation : element_allocations_)
                {
                    allocation.module->element_expr_funcref_vec_storage.resize(allocation.function_count);
                    allocation.module->element_expr_externref_vec_storage.resize(allocation.opaque_count);
                    allocation.module->element_expr_gc_ref_vec_storage.resize(allocation.gc_count);
                }
                // Fixed completion roster BEFORE any resource is mutated. Only
                // successfully filled actual owned records can occupy a slot.
                expected_resource_records_ = count; resource_fixups_started_ = true; return true;
            }
            [[nodiscard]] ::std::size_t completion_index(void const* actual) const noexcept
            {
                if(actual == nullptr) { return SIZE_MAX; }
                auto const address{reinterpret_cast<::std::uintptr_t>(actual)};
                ::std::size_t begin{},end{completed_resource_records_.size()};
                while(begin < end)
                {
                    auto const middle{begin+(end-begin)/2u}; // middle<end<=fixedsize before indexed read.
                    // [FINAL sorted native comparison roster begin..end) end
                    // [safe] keys only, no supplied address/pointee dereference.
                    if(completed_resource_records_[middle].address < address) { begin=middle+1u; }
                    else { end=middle; }
                }
                return begin < completed_resource_records_.size() && completed_resource_records_[begin].address == address ? begin : SIZE_MAX;
            }
            [[nodiscard]] bool record_completed(void const* actual) const noexcept
            {
                auto const index{completion_index(actual)};
                return index < completed_resource_records_.size() && completed_resource_records_[index].completed;
            }
            [[nodiscard]] bool note_completed(void const* actual) noexcept
            {
                auto const index{completion_index(actual)};
                if(index >= completed_resource_records_.size() || completed_resource_count_ >= expected_resource_records_ ||
                    completed_resource_records_[index].completed) { return false; }
                // Exact immutable roster membership precedes the only bit change;
                // no append/allocation or record relocation occurs during fill.
                completed_resource_records_[index].completed=true; ++completed_resource_count_; return true;
            }
            [[nodiscard]] bool canonical_unpublished_function_reference(reference candidate,reference& result) noexcept
            {
                namespace storage=::uwvm2::uwvm::runtime::storage;
                using kind=::uwvm2::object::global::wasm_ref_kind;
                using imported_kind=storage::imported_function_link_kind;
                if(state_ != skeleton_result::built || !resource_fixups_started_ || !source_ || source_->initialized_ ||
                    !source_type::has_canonical_owner(source_)) { return false; }
                if(candidate.kind == kind::wasm_func_defined)
                {
                    auto const location{details::locate_linked_defined_function_in_context(world_,
                        static_cast<storage::local_defined_function_storage_t const*>(candidate.storage.ptr))};
                    if(location.module == nullptr || location.index >= location.module->local_defined_function_vec_storage.size()) { return false; }
                    result={};result.kind=kind::wasm_func_defined;
                    result.storage.ptr=const_cast<storage::local_defined_function_storage_t*>(
                        ::std::addressof(location.module->local_defined_function_vec_storage.index_unchecked(location.index)));
                    return true;
                }
                if(candidate.kind != kind::wasm_func_imported) { return false; }
                auto const* next{static_cast<storage::imported_function_storage_t const*>(candidate.storage.ptr)};
                for(::std::size_t hop{}; hop < function_import_hop_limit_; ++hop)
                {
                    auto const location{details::locate_linked_imported_function_in_context(world_,next)};
                    if(location.module == nullptr || location.index >= location.module->imported_function_vec_storage.size()) { return false; }
                    // [actual FINAL source-owned import vector, index<size] end
                    // [safe] exact aligned membership BEFORE reading active union.
                    auto const& actual{location.module->imported_function_vec_storage.index_unchecked(location.index)};
                    if(actual.link_kind == imported_kind::defined)
                    {
                        reference defined{};defined.kind=kind::wasm_func_defined;
                        defined.storage.ptr=const_cast<storage::local_defined_function_storage_t*>(actual.target.defined_ptr);
                        return canonical_unpublished_function_reference(defined,result);
                    }
                    if(actual.link_kind == imported_kind::local_imported)
                    {
                        // Exact terminal record identity only; no provider/name/
                        // signature guess is made and no effect permission issued.
                        result={};result.kind=kind::wasm_func_imported;
                        result.storage.ptr=const_cast<storage::imported_function_storage_t*>(::std::addressof(actual));return true;
                    }
                    if(actual.link_kind != imported_kind::imported) { return false; }
                    next=actual.target.imported_ptr; // compare membership next hop BEFORE any pointee read.
                }
                return false; // finite total imports covers every valid chain; cycles reject.
            }
            template<typename Record, typename Selector>
            [[nodiscard]] storage_module* locate_unpublished_record(Record const* candidate, Selector select,
                ::std::size_t& local) noexcept
            {
                if(state_ != skeleton_result::built || !resource_fixups_started_ || !source_ || source_->initialized_ ||
                    candidate == nullptr || record_completed(candidate)) { return nullptr; }
                for(auto& [name, module] : source_->registry_)
                {
                    static_cast<void>(name);
                    // [actual FINAL source registry][its actual bounded record vector]
                    // [safe] supplied candidate address is only COMPARED by exact
                    // aligned element membership before any candidate pointee read.
                    if(details::linked_function_record_index(candidate, select(module), local)) { return ::std::addressof(module); }
                }
                return nullptr;
            }
            [[nodiscard]] bool stage_saved_memory_image(
                ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t* candidate,
                unsigned address_bits, bool shared, ::std::uint64_t pages, ::std::uint64_t minimum,
                ::std::uint64_t maximum, ::std::span<saved_memory_chunk const> chunks,
                ::std::size_t maximum_native_bytes) noexcept
            {
                ::std::size_t local{};
                auto* module{locate_unpublished_record(candidate, [](auto& m)->auto& { return m.local_defined_memory_vec_storage; }, local)};
                if(module == nullptr) { return false; }
                // [actual owned metadata-only memory record][real immutable type]
                // [safe] candidate was authenticated to the actual vector above.
                auto& actual{module->local_defined_memory_vec_storage.index_unchecked(local)};
                if(actual.memory_type_ptr == nullptr || actual.memory.memory_begin != nullptr ||
                    actual.memory.custom_page_size_log2 != 16u || chunks.size() >
                    static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(saved_memory_chunk) ||
                    (!chunks.empty() && chunks.data() == nullptr)) { return false; }
                auto const& declaration{*actual.memory_type_ptr};
                // Actual configured overrides replace declared min/max,
                // including accepted warned widening; cold census uses the SAME
                // pure address-domain projection, never a second intersection.
                auto const saved_bounds{::uwvm2::uwvm::runtime::storage::checkpoint_effective_memory_bounds(
                    declaration.address64, actual.effective_limits)};
                auto const real_min{saved_bounds.minimum}, real_max{saved_bounds.maximum};
                if(address_bits != (declaration.address64?64u:32u) || shared != declaration.shared ||
                    (shared && !declaration.limits.present_max) || minimum != real_min || maximum != real_max ||
                    pages < minimum || pages > maximum || pages > maximum_native_bytes/65536u ||
                    pages > static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/65536u) { return false; }
                auto const bytes{static_cast<::std::size_t>(pages)*65536u}; // both native/ptrdiff quotients checked BEFORE multiplication.
                ::std::uint64_t last_end{}; bool seen{};
                for(auto const& chunk : chunks)
                {
                    if(chunk.bytes.empty() || chunk.bytes.data() == nullptr || chunk.offset > bytes ||
                        chunk.bytes.size() > bytes-static_cast<::std::size_t>(chunk.offset) ||
                        chunk.bytes.size() > UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(chunk.bytes.data()) ||
                        (seen && chunk.offset < last_end)) { return false; }
                    last_end=chunk.offset+chunk.bytes.size(); seen=true; // bounded by bytes BEFORE addition.
                }
                auto initialize = [&]<typename Memory>(Memory& memory) noexcept
                {
                    if(shared && !Memory::support_multi_thread) { return false; }
                    if constexpr(Memory::can_mmap)
                    {
                        // Match the allocator's actual reservation ceiling before
                        // invoking its fatal-on-invalid native initialization API.
                        constexpr ::std::uint64_t host64_wasm32{(::std::uint64_t{1u}<<32u)};
                        constexpr ::std::uint64_t host64_wasm64{(::std::uint64_t{1u}<<40u)};
                        constexpr ::std::uint64_t host32_memory{(::std::uint64_t{1u}<<28u)};
                        auto const ceiling{sizeof(::std::size_t)>=8u ? (declaration.address64?host64_wasm64:host64_wasm32) : host32_memory};
                        if(bytes > ceiling || bytes > memory.reservation_limit_bytes) { return false; }
                    }
                    auto restored_limits{actual.effective_limits}; restored_limits.min=static_cast<::std::size_t>(pages);
                    if(module->imported_memory_vec_storage.size() > SIZE_MAX-local) { return false; }
                    details::initialize_native_memory(memory, restored_limits,
                        module->imported_memory_vec_storage.size()+local, shared, declaration.address64);
                    if((bytes!=0u && memory.memory_begin==nullptr) ||
                        bytes > UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(memory.memory_begin)) { return false; }
                    for(auto const& chunk : chunks)
                    {
                        // [actual private zero-filled saved allocation][offset, offset+size) end
                        // [safe] ALL chunk/source/native extents and quota were
                        // preflighted before allocation; derive destination +offset
                        // only within that owned committed extent. No access guard
                        // or lock is added to any generated memory instruction.
                        ::fast_io::freestanding::my_memcpy(memory.memory_begin+static_cast<::std::size_t>(chunk.offset),
                            chunk.bytes.data(), chunk.bytes.size());
                    }
                    return true;
                };
                return initialize(actual.memory) && note_completed(::std::addressof(actual));
            }
            [[nodiscard]] bool stage_saved_numeric_global(
                ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t* candidate,
                saved_core_type expected, bool mutable_value, native_value const& complete_native_bits) noexcept
            {
                ::std::size_t local{};
                auto* module{locate_unpublished_record(candidate, [](auto& m)->auto& { return m.local_defined_global_vec_storage; }, local)};
                if(module==nullptr) { return false; }
                auto& actual{module->local_defined_global_vec_storage.index_unchecked(local)};
                if(actual.global_type_ptr==nullptr || actual.init_state!=::uwvm2::uwvm::runtime::storage::wasm_global_init_state::uninitialized ||
                    actual.global.is_mutable!=mutable_value || expected.kind==saved_core_kind::reference ||
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(*actual.global_type_ptr)!=expected)
                { return false; }
                using kind=::uwvm2::object::global::global_type;
                // All payloads are predecoded raw native IEEE/integer/lane bytes;
                // NEVER evaluate a floating value while restoring NaN/sign bits.
                // [complete_native_bits: fixed 16 bytes][actual typed union member]
                // [safe] below fixed 4/8/16 widths are <= native_value::bits and
                // match the authenticated actual union member BEFORE memcpy.
                // No pointer increment, byte-offset or floating read is needed.
                switch(expected.kind)
                {
                    case saved_core_kind::i32: if(actual.global.kind!=kind::wasm_i32) { return false; }
                        ::fast_io::freestanding::my_memcpy(::std::addressof(actual.global.storage.i32), complete_native_bits.bits.data(),4u); break;
                    case saved_core_kind::f32: if(actual.global.kind!=kind::wasm_f32) { return false; }
                        ::fast_io::freestanding::my_memcpy(::std::addressof(actual.global.storage.f32), complete_native_bits.bits.data(),4u); break;
                    case saved_core_kind::i64: if(actual.global.kind!=kind::wasm_i64) { return false; }
                        ::fast_io::freestanding::my_memcpy(::std::addressof(actual.global.storage.i64), complete_native_bits.bits.data(),8u); break;
                    case saved_core_kind::f64: if(actual.global.kind!=kind::wasm_f64) { return false; }
                        ::fast_io::freestanding::my_memcpy(::std::addressof(actual.global.storage.f64), complete_native_bits.bits.data(),8u); break;
                    case saved_core_kind::v128: if(actual.global.kind!=kind::wasm_v128) { return false; }
                        ::fast_io::freestanding::my_memcpy(::std::addressof(actual.global.storage.v128), complete_native_bits.bits.data(),16u); break;
                    default: return false;
                }
                actual.init_state=::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                return note_completed(::std::addressof(actual));
            }
            [[nodiscard]] bool stage_saved_reference_global(
                ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t* candidate,
                saved_core_type expected, bool mutable_value, reference const& relocated) noexcept
            {
                ::std::size_t local{};
                auto* module{locate_unpublished_record(candidate, [](auto& m)->auto& { return m.local_defined_global_vec_storage; }, local)};
                if(module==nullptr) { return false; }
                auto& actual{module->local_defined_global_vec_storage.index_unchecked(local)};
                if(actual.global_type_ptr==nullptr || actual.init_state!=::uwvm2::uwvm::runtime::storage::wasm_global_init_state::uninitialized ||
                    actual.global.kind!=::uwvm2::object::global::global_type::wasm_ref || actual.global.is_mutable!=mutable_value ||
                    actual.global.ref_lease_store!=module->gc_store.get() || expected.kind!=saved_core_kind::reference ||
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(*actual.global_type_ptr)!=expected ||
                    !staged_gc_.pending_value_matches(native_value::reference(relocated), {expected}, module->gc_store) ||
                    staged_gc_.retain_staged_reference(module->gc_store, relocated)!=::uwvm2::uwvm::runtime::storage::gc_object_status::ok)
                { return false; }
                actual.global.storage.ref=relocated;
                actual.init_state=::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                return note_completed(::std::addressof(actual));
            }
            [[nodiscard]] static bool saved_reference_declaration(auto const& declaration, saved_core_type& expected) noexcept
            {
                expected={}; expected.kind=saved_core_kind::reference; expected.nullable=true;
                if(declaration.has_core_type) { expected=declaration.core_type; return expected.kind==saved_core_kind::reference; }
                using heap=::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
                switch(static_cast<unsigned>(declaration.reftype))
                {
                    case 0x70u: expected.heap.code=static_cast<::std::int_least64_t>(heap::func); return true;
                    case 0x6fu: expected.heap.code=static_cast<::std::int_least64_t>(heap::extern_); return true;
                    case 0x69u: expected.heap.code=static_cast<::std::int_least64_t>(heap::exn); return true;
                    default: return false;
                }
            }
            [[nodiscard]] bool stage_saved_table_image(
                ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* candidate,
                unsigned address_bits, ::std::uint64_t length, ::std::uint64_t minimum, ::std::uint64_t maximum,
                saved_core_type expected, reference const& default_value,
                ::std::span<saved_table_chunk const> chunks, ::std::size_t maximum_native_bytes) noexcept
            {
                namespace storage=::uwvm2::uwvm::runtime::storage;
                ::std::size_t local{};
                auto* module{locate_unpublished_record(candidate, [](auto& m)->auto& { return m.local_defined_table_vec_storage; }, local)};
                if(module==nullptr) { return false; }
                auto& actual{module->local_defined_table_vec_storage.index_unchecked(local)};
                if(actual.table_type_ptr==nullptr || actual.owner_module_rt_ptr!=module || !actual.elems.empty() ||
                    chunks.size()>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(saved_table_chunk) ||
                    (!chunks.empty() && chunks.data()==nullptr)) { return false; }
                auto const& declaration{*actual.table_type_ptr}; saved_core_type real_type{};
                auto const real_max{(::std::min)(declaration.address64 ? (::std::numeric_limits<::std::uint64_t>::max)() :
                    ::std::uint64_t{0xffffffffu}, static_cast<::std::uint64_t>(declaration.limits.max))};
                if(!saved_reference_declaration(declaration,real_type) || expected!=real_type ||
                    address_bits!=(declaration.address64?64u:32u) || minimum!=declaration.limits.min || maximum!=real_max ||
                    length<minimum || length>maximum || length>maximum_native_bytes/sizeof(storage::local_defined_table_elem_storage_t) ||
                    length>static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(storage::local_defined_table_elem_storage_t))
                { return false; }
                auto accept_reference = [&](reference const& relocated) noexcept
                {
                    return staged_gc_.pending_value_matches(native_value::reference(relocated), {expected}, module->gc_store) &&
                        staged_gc_.retain_staged_reference(module->gc_store,relocated)==storage::gc_object_status::ok;
                };
                // Empty nonnullable tables have only a declared descriptor: do
                // not invent or validate a null DEFAULT VALUE that does not exist.
                if(length!=0u && !accept_reference(default_value)) { return false; }
                ::std::uint64_t last_end{}; bool seen{};
                for(auto const& chunk : chunks)
                {
                    if(chunk.values.empty() || chunk.values.data()==nullptr || chunk.offset>length ||
                        chunk.values.size()>length-chunk.offset || chunk.values.size()>
                        static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(reference) ||
                        chunk.values.size()> (UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(chunk.values.data()))/sizeof(reference) ||
                        (seen && chunk.offset<last_end)) { return false; }
                    for(auto const& relocated : chunk.values) { if(!accept_reference(relocated)) { return false; } }
                    last_end=chunk.offset+chunk.values.size(); seen=true; // bounded by saved length before sum.
                }
                storage::local_defined_table_elem_storage_t fill{};
                if(length!=0u) { fill=storage::runtime_table_slot_from_gc_reference(default_value); }
                if(!storage::try_grow_table_elements(actual,static_cast<::std::size_t>(length),fill)) { return false; }
                for(auto const& chunk : chunks)
                {
                    auto const start{static_cast<::std::size_t>(chunk.offset)};
                    for(::std::size_t n{}; n!=chunk.values.size(); ++n)
                    {
                        // [actual new table, saved checked range start+n<length] end
                        // [safe] complete type/root/host-allocation/offset preflight
                        // precedes unchecked slot access. No published table view
                        // is notified or refreshed while this world is private.
                        actual.elems.index_unchecked(start+n)=storage::runtime_table_slot_from_gc_reference(chunk.values[n]);
                    }
                }
                return note_completed(::std::addressof(actual));
            }
            [[nodiscard]] ::uwvm2::uwvm::wasm::type::wasm_file_t const* unpublished_file_for_module(
                storage_module const* candidate) const noexcept
            {
                source_type::owner actual{source_};
                return actual_unpublished_file(actual, candidate);
            }
            [[nodiscard]] static bool owned_file_byte_slice(::uwvm2::uwvm::wasm::type::wasm_file_t const& file,
                ::std::byte const* first, ::std::byte const* last, ::std::size_t& count) noexcept
            {
                count=0u;
                if(!file.has_owned_source_image() || file.binfmt_ver!=1u) { return false; }
                auto const size{file.source_size()},base{reinterpret_cast<::std::uintptr_t>(file.source_cbegin())};
                auto const begin{reinterpret_cast<::std::uintptr_t>(first)},end{reinterpret_cast<::std::uintptr_t>(last)};
                if(size>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
                    size>UINTPTR_MAX-base || begin<base || end<begin || end-base>size) { return false; }
                count=static_cast<::std::size_t>(end-begin); return true;
            }
            [[nodiscard]] bool stage_saved_data_segment(
                ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t* candidate,
                bool dropped, ::std::span<::std::byte const> saved_bytes) noexcept
            {
                namespace storage=::uwvm2::uwvm::runtime::storage;
                ::std::size_t local{};
                auto* module{locate_unpublished_record(candidate, [](auto& m)->auto& { return m.local_defined_data_vec_storage; }, local)};
                if(module==nullptr) { return false; }
                auto& actual{module->local_defined_data_vec_storage.index_unchecked(local)};
                auto const* file{unpublished_file_for_module(module)};
                if(file==nullptr || actual.data_type_ptr==nullptr ||
                    saved_bytes.size()>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
                    (!saved_bytes.empty() && saved_bytes.data()==nullptr) ||
                    saved_bytes.size()>UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(saved_bytes.data())) { return false; }
                if(dropped)
                {
                    if(!saved_bytes.empty()) { return false; }
                    actual.data.dropped=true; return note_completed(::std::addressof(actual));
                }
                ::std::size_t count{};
                if(actual.data.kind!=storage::wasm_data_segment_kind::passive ||
                    !owned_file_byte_slice(*file,actual.data.byte_begin,actual.data.byte_end,count) || count!=saved_bytes.size())
                { return false; }
                // Passive data contents cannot change during Wasm execution;
                // only drop changes state. Match saved contents to the actual
                // exclusive ORIGINAL source payload rather than adopting a
                // wire address, mutating parser bytes or allocating another owner.
                for(::std::size_t n{}; n!=count; ++n)
                {
                    // [actual source-owned payload][actual saved span] count
                    // [safe] complete byte extents and identities BEFORE n read.
                    if(actual.data.byte_begin[n]!=saved_bytes[n]) { return false; }
                }
                actual.data.dropped=false; return note_completed(::std::addressof(actual));
            }
            [[nodiscard]] bool stage_saved_element_segment(
                ::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t* candidate,
                bool dropped, saved_core_type expected, ::std::span<reference const> saved_values) noexcept
            {
                namespace storage = ::uwvm2::uwvm::runtime::storage;
                using family = storage::runtime_table_reference_family;
                ::std::size_t local{};
                auto* module{locate_unpublished_record(candidate, [](auto& m)->auto& { return m.local_defined_element_vec_storage; }, local)};
                if(module == nullptr) { return false; }
                auto& actual{module->local_defined_element_vec_storage.index_unchecked(local)};
                if(actual.element_type_ptr == nullptr || unpublished_file_for_module(module) == nullptr ||
                   saved_values.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(reference) ||
                   (!saved_values.empty() && saved_values.data() == nullptr) ||
                   saved_values.size() > (UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(saved_values.data()))/sizeof(reference))
                { return false; }
                // [actual context-created record][owned parsed element declaration]
                // [safe] actual record membership and owning file above BEFORE
                // reading its trusted immutable declaration; input span is actual
                // relocated native DATA, never a wire pointer/persisted native bits.
                auto const& declaration{actual.element_type_ptr->storage.segment}; saved_core_type real_type{};
                if(!saved_reference_declaration(declaration, real_type) || expected != real_type) { return false; }
                if(dropped)
                {
                    if(!saved_values.empty()) { return false; }
                    // A dropped segment is logically empty. Its dormant immutable
                    // source payload need not be replayed/evaluated a second time.
                    actual.element.dropped = true; return note_completed(::std::addressof(actual));
                }
                if(declaration.active || declaration.declarative || actual.element.kind != storage::wasm_element_segment_kind::passive)
                { return false; }
                auto const expressions{declaration.vec_expr.size()}, indices{declaration.vec_funcidx.size()};
                if((expressions != 0u && indices != 0u) || saved_values.size() != (expressions == 0u ? indices : expressions))
                { return false; }
                auto const kind{storage::runtime_element_family(actual, *module)};
                element_allocation* allocation{};
                for(auto& current : element_allocations_) { if(current.module == module) { allocation = ::std::addressof(current); break; } }
                if(allocation == nullptr) { return false; }
                auto accept = [&](reference const& relocated) noexcept
                {
                    return staged_gc_.pending_value_matches(native_value::reference(relocated), {expected}, module->gc_store) &&
                        staged_gc_.retain_staged_reference(module->gc_store, relocated) == storage::gc_object_status::ok;
                };
                if(expressions == 0u)
                {
                    // Index-form passive elements never change their contents.
                    // Authenticate the saved logical funcref against the exact new
                    // module's real function record selected by ORIGINAL indices.
                    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
                    auto const imports{module->imported_function_vec_storage.size()};
                    for(::std::size_t n{}; n != indices; ++n)
                    {
                        // [FINAL original index vector][checked saved native span] end
                        // [safe] n<indices and exact equal extents before both reads.
                        auto const index{static_cast<::std::size_t>(declaration.vec_funcidx.index_unchecked(n))};
                        reference actual_reference{};
                        if(index < imports)
                        {
                            actual_reference.kind = ref_kind::wasm_func_imported;
                            actual_reference.storage.ptr = ::std::addressof(module->imported_function_vec_storage.index_unchecked(index));
                        }
                        else
                        {
                            auto const defined{index-imports};
                            if(defined >= module->local_defined_function_vec_storage.size()) { return false; }
                            // [actual local functions] end
                            // [safe] public-index subtraction and local extent BEFORE
                            // deriving this genuine relocated function record address.
                            actual_reference.kind = ref_kind::wasm_func_defined;
                            actual_reference.storage.ptr = ::std::addressof(module->local_defined_function_vec_storage.index_unchecked(defined));
                        }
                        reference canonical_saved{},canonical_original{};
                        if(!canonical_unpublished_function_reference(actual_reference,canonical_original) ||
                           !canonical_unpublished_function_reference(saved_values[n],canonical_saved) ||
                           canonical_saved.kind != canonical_original.kind || canonical_saved.storage.ptr != canonical_original.storage.ptr ||
                           !accept(canonical_saved)) { return false; }
                    }
                    // The skeleton already borrows these immutable source indices.
                    // No original element expression/active initializer is rerun.
                    actual.element.dropped = false; return note_completed(::std::addressof(actual));
                }
                auto& cursor{kind == family::function ? allocation->function_cursor :
                    (kind == family::external || kind == family::exception ? allocation->opaque_cursor : allocation->gc_cursor)};
                auto const count{kind == family::function ? allocation->function_count :
                    (kind == family::external || kind == family::exception ? allocation->opaque_count : allocation->gc_count)};
                if(cursor > count || expressions > count-cursor) { return false; }
                if((kind == family::function && module->element_expr_funcref_vec_storage.size() != count) ||
                   ((kind == family::external || kind == family::exception) && module->element_expr_externref_vec_storage.size() != count) ||
                   (kind == family::gc && module->element_expr_gc_ref_vec_storage.size() != count)) { return false; }
                for(auto const& relocated : saved_values) { if(!accept(relocated)) { return false; } }
                auto const first{cursor};
                // Clear dormant source-form selectors ONLY after complete saved
                // type/reference owner preflight. Only one representation is live.
                actual.element.funcidx_begin = actual.element.funcidx_end = nullptr;
                actual.element.funcref_begin = actual.element.funcref_end = nullptr;
                actual.element.externref_begin = actual.element.externref_end = nullptr;
                actual.element.gc_ref_begin = actual.element.gc_ref_end = nullptr;
                if(kind == family::function)
                {
                    auto& owner{module->element_expr_funcref_vec_storage};
                    if(owner.size() != count) { return false; }
                    for(::std::size_t n{}; n != expressions; ++n)
                    {
                        // [FINAL preallocated family owner][first+n<count] end
                        // [safe] expressions<=count-first BEFORE every unchecked
                        // slot read/write; native source span has same checked size.
                        owner.index_unchecked(first+n) = storage::runtime_table_slot_from_gc_reference(saved_values[n]);
                    }
                    // [FINAL owner, exact checked nonempty range] one-past
                    // [safe] +first and +expressions remain in this fixed owner;
                    // this vector cannot grow after begin_resource_fixups.
                    actual.element.funcref_begin = owner.data()+first;
                    actual.element.funcref_end = actual.element.funcref_begin+expressions;
                }
                else if(kind == family::external || kind == family::exception)
                {
                    auto& owner{module->element_expr_externref_vec_storage};
                    if(owner.size() != count) { return false; }
                    for(::std::size_t n{}; n != expressions; ++n)
                    {
                        // [FINAL preallocated family owner][first+n<count] end
                        // [safe] complete extent/types/real staged token ownership
                        // checked BEFORE payload access; carriers are NEVER read as
                        // native object addresses. Root recipients retained above.
                        owner.index_unchecked(first+n) = saved_values[n].storage.ptr;
                    }
                    // [FINAL owner, exact checked nonempty range] one-past
                    // [safe] +first then +expressions bounded by fixed allocation.
                    actual.element.externref_begin = owner.data()+first;
                    actual.element.externref_end = actual.element.externref_begin+expressions;
                }
                else
                {
                    auto& owner{module->element_expr_gc_ref_vec_storage};
                    if(owner.size() != count) { return false; }
                    for(::std::size_t n{}; n != expressions; ++n)
                    {
                        // [FINAL full reference owner][first+n<count] end
                        // [safe] complete bounded payload; preserve i31 kind/bits,
                        // relocated aggregates and strong recipient root bindings.
                        owner.index_unchecked(first+n) = saved_values[n];
                    }
                    // [FINAL owner, exact checked nonempty range] one-past
                    // [safe] fixed count preflight before both pointer derivations.
                    actual.element.gc_ref_begin = owner.data()+first;
                    actual.element.gc_ref_end = actual.element.gc_ref_begin+expressions;
                }
                cursor += expressions; // bounded by count-cursor BEFORE sum.
                actual.element.dropped = false; return note_completed(::std::addressof(actual));
            }
            [[nodiscard]] bool all_actual_resource_records_filled() const noexcept
            {
                // PRIVATE DATA consistency only, not a future epoch/execution or
                // publication permit. Genuine world issuer still owns source/body/
                // graph correspondence, every GC shell+host/exception adapter,
                // ALL oldworld drain and atomic new publication requirements.
                if(state_ != skeleton_result::built || !source_ || source_->initialized_ || !resource_fixups_started_ ||
                    completed_resource_records_.size() != expected_resource_records_ ||
                    completed_resource_count_ != expected_resource_records_) { return false; }
                for(auto const& [name, module] : source_->registry_)
                {
                    static_cast<void>(name);
                    auto complete = [&](auto const& records) noexcept
                    {
                        for(auto const& actual : records) { if(!record_completed(::std::addressof(actual))) { return false; } }
                        return true;
                    };
                    if(unpublished_file_for_module(::std::addressof(module)) == nullptr ||
                       !complete(module.local_defined_memory_vec_storage) || !complete(module.local_defined_table_vec_storage) ||
                       !complete(module.local_defined_global_vec_storage) || !complete(module.local_defined_data_vec_storage) ||
                       !complete(module.local_defined_element_vec_storage)) { return false; }
                }
                return staged_gc_.publication_preflight();
            }
            // Source-owned rewritten import names survive this lexical context.
            // These are copies of trusted native policy, never file-supplied callbacks.
            void copy_native_policy()
            {
                auto& resets{source_->checkpoint_import_resets_};
                for(auto const& [name, actual] : ::uwvm2::uwvm::wasm::storage::configured_module_import_reset)
                {
                    auto const inserted{resets.try_emplace(::uwvm2::utils::container::u8string{name})};
                    auto& copied{inserted.first->second}; copied.reserve(actual.size());
                    for(auto const& rule : actual)
                    {
                        copied.push_back(::uwvm2::uwvm::wasm::storage::configured_import_reset_t{
                            rule.import_module_name, rule.import_extern_name,
                            rule.new_import_module_name, rule.new_import_extern_name, 0uz});
                    }
                }
                auto& memory{source_->checkpoint_memory_limits_};
                for(auto const& [name, actual] : ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit)
                {
                    auto const inserted{memory.try_emplace(::uwvm2::utils::container::u8string{name})};
                    auto& copied{inserted.first->second}; copied.all_limits = actual.all_limits;
                    copied.apply_to_all_memories = actual.apply_to_all_memories;
                    for(auto const& [index, limit] : actual.local_defined_memory_limits)
                    { copied.local_defined_memory_limits.try_emplace(index, limit); }
                }
            }
            [[nodiscard]] skeleton_result build_resource_skeleton()
            {
                if(state_ != skeleton_result::empty) { return state_; }
                auto const& main{source_->file_};
                if(main.binfmt_ver != 1u || !main.has_owned_source_image())
                { return state_ = skeleton_result::invalid_source; }
                // Count actual source and native adapter declarations BEFORE allocating
                // any new map buckets. The source factory bounded preload extent to 4095.
                auto const wasm_count{source_->preload_files_.size() + 1uz};
                auto declared_count{wasm_count};
                for(auto const& [name, adapter] : ::uwvm2::uwvm::wasm::storage::all_module)
                {
                    static_cast<void>(name);
                    using kind = ::uwvm2::uwvm::wasm::type::module_type_t;
                    if(adapter.type == kind::exec_wasm || adapter.type == kind::preloaded_wasm) { continue; }
                    if(declared_count == (::std::numeric_limits<::std::size_t>::max)())
                    { return state_ = skeleton_result::invalid_source; }
                    ++declared_count; // actual count proved representable BEFORE increment.
                }
                details::check_reserve_limit_in_context(world_, u8"declaration_modules", declared_count, limits_.max_runtime_modules);
                details::check_reserve_limit_in_context(world_, u8"runtime_modules", wasm_count, limits_.max_runtime_modules);
                auto& declared{source_->checkpoint_declarations_};
                auto insert = [&](auto const& file, ::uwvm2::uwvm::wasm::type::module_type_t kind) noexcept
                {
                    if(file.binfmt_ver != 1u || !file.has_owned_source_image()) { return false; }
                    return declared.try_emplace(file.module_name, ::uwvm2::uwvm::wasm::type::all_module_t{
                        .module_storage_ptr = {.wf = ::std::addressof(file)}, .type = kind}).second;
                };
                if(!insert(main, ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm))
                { return state_ = skeleton_result::duplicate_module; }
                for(auto const& file : source_->preload_files_)
                {
                    // [FINAL parsed owned preload array] end
                    // [safe] range-for observes actual source members only; no guest address/ID.
                    if(!insert(file, ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm))
                    { return state_ = skeleton_result::duplicate_module; }
                }
                // The actual transaction must hold the native environment maintenance
                // boundary while borrowing existing import adapters. Reuse only actual
                // native declarations; no host init routine or guest start executes.
                // Host state restoration/replay is a separate actual adapter obligation.
                for(auto const& [name, adapter] : ::uwvm2::uwvm::wasm::storage::all_module)
                {
                    using kind = ::uwvm2::uwvm::wasm::type::module_type_t;
                    if(adapter.type == kind::exec_wasm || adapter.type == kind::preloaded_wasm) { continue; }
                    if(!declared.try_emplace(name, adapter).second)
                    { return state_ = skeleton_result::duplicate_module; }
                }
                details::check_reserve_limit_in_context(world_, u8"import_reset_modules",
                    ::uwvm2::uwvm::wasm::storage::configured_module_import_reset.size(), limits_.max_runtime_modules);
                details::check_reserve_limit_in_context(world_, u8"memory_limit_modules",
                    ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit.size(), limits_.max_runtime_modules);
                copy_native_policy();
                auto const dependencies{::uwvm2::uwvm::wasm::loader::
                    build_dependency_graph_and_check_import_exist_and_construct_all_module_export_in_context(world_)};
                if(dependencies.ok != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
                { return state_ = skeleton_result::dependency_error; }
                auto& modules{world_.modules()};
                modules.reserve(wasm_count);
                for(auto const& [name, declaration] : declared)
                {
                    using kind = ::uwvm2::uwvm::wasm::type::module_type_t;
                    if(declaration.type != kind::exec_wasm && declaration.type != kind::preloaded_wasm) { continue; }
                    current_module_ = name;
                    storage_module module{};
                    details::initialize_from_wasm_file_in_context(world_, *declaration.module_storage_ptr.wf, module);
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    module.module_name = name;
#endif
                    if(!modules.try_emplace(name, ::std::move(module)).second)
                    { current_module_ = {}; return state_ = skeleton_result::duplicate_module; }
                }
                current_module_ = {};
                details::validate_configured_runtime_memory_limit_target_modules_in_context(world_);
                details::validate_configured_import_reset_target_modules_in_context(world_);
                details::validate_configured_import_reset_matches_in_context(world_);
                details::resolve_imports_for_wasm_file_modules_in_context(world_);
                details::error_on_unresolved_imports_after_linking_in_context(world_);
                details::validate_and_resolve_core3_tags_after_linking_in_context(world_);
                details::validate_wasm_file_module_import_types_after_linking_in_context(world_);
                // Actual in-map addresses are now stable. No original global/table
                // initializer runs and no ref value is read from an uninitialized slot.
                for(auto& [name, module] : modules)
                {
                    static_cast<void>(name);
                    for(auto& table : module.local_defined_table_vec_storage)
                    { table.owner_module_rt_ptr = ::std::addressof(module); }
                    for(auto& global : module.local_defined_global_vec_storage)
                    { global.owner_module_rt_ptr = ::std::addressof(module); }
                }
                // Deliberately no phase/source seal, initializer serial, global
                // selector, native publication, active segment/drop, or guest start.
                // Only a real joint graph/fixup completion consumed by the private
                // world transaction can later authorize compile and actual commit.
                return state_ = skeleton_result::built;
            }
            [[nodiscard]] ::std::shared_ptr<store_type> create_store(
                ::uwvm2::uwvm::runtime::storage::gc_type::recursive_type_section const& section,
                ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const& leases) noexcept
            {
                source_type::owner actual{source_};
                return staged_gc_.create_store(section, leases, linked_function_type_matches, actual);
            }
            [[nodiscard]] static bool linked_function_type_matches(reference actual,
                store_type const* expected, ::std::uint_least32_t type_index) noexcept
            {
                if(expected == nullptr) { return false; }
                // Expected is supplied only by the native store's trusted immutable
                // callback. Weak lifetime identity is not sufficient authority: prove
                // canonical source owner AND actual store membership before type reads.
                auto const source{expected->checkpoint_source_binding_.lock()};
                if(!source_type::has_canonical_owner(source)) { return false; }
                bool member{};
                for(auto const& [name, module] : source->registry_)
                { static_cast<void>(name); if(module.gc_store.get() == expected) { member = true; break; } }
                if(!member) { return false; }
                ::uwvm2::utils::container::u8string_view diagnostic{}; bool sane{};
                initializer_limit_t cap{initializer_limit};
                // The source itself retains final maps. No expired context pointer
                // is captured in generated functions/store callbacks after commit.
                // This private read-only borrow never invokes make_store/mutators.
                auto& native{const_cast<source_type&>(*source)};
                details::initialization_context<purpose> world{native.registry_, native.checkpoint_declarations_,
                    native.checkpoint_exports_, native.checkpoint_import_resets_, native.checkpoint_memory_limits_,
                    diagnostic, sane, cap, nullptr};
                if(type_index == (::std::numeric_limits<::std::uint_least32_t>::max)())
                {
                    // Private staging convention ONLY: authenticate abstract funcref
                    // membership without claiming a declared defined heap type.
                    // [address token] no pointee read before actual vector authentication.
                    using kind = ::uwvm2::object::global::wasm_ref_kind;
                    if(actual.storage.ptr == nullptr) { return false; }
                    if(actual.kind == kind::wasm_func_defined)
                    {
                        auto const location{details::locate_linked_defined_function_in_context(world,
                            static_cast<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const*>(actual.storage.ptr))};
                        return location.module != nullptr;
                    }
                    if(actual.kind == kind::wasm_func_imported)
                    {
                        auto const* candidate{static_cast<::uwvm2::uwvm::runtime::storage::imported_function_storage_t const*>(actual.storage.ptr)};
                        auto const location{details::locate_linked_imported_function_in_context(world, candidate)};
                        if(location.module == nullptr) { return false; }
                        auto const leaf{details::resolve_linked_imported_function_leaf_in_context(world, candidate)};
                        return leaf.imported != nullptr;
                    }
                    return false;
                }
                return details::linked_gc_function_reference_type_matches_in_context(world, actual, expected, type_index);
            }
        public:
            restoration_context(restoration_context const&) = delete;
            restoration_context& operator=(restoration_context const&) = delete;
            restoration_context(restoration_context&&) = delete;
            restoration_context& operator=(restoration_context&&) = delete;
            ~restoration_context() = default;
        };
    }
    extern "C++"
    {
        inline bool staged_compiler_module_owner::matches_unpublished_file() const noexcept
        {
            return file_ != nullptr && restoration_context::actual_unpublished_file(source_, module_) == file_;
        }
    }
    namespace details
    {
        inline ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store>
        initialization_context<initialization_purpose::ordinary>::make_store(
            ::uwvm2::uwvm::runtime::storage::gc_type::recursive_type_section const& section,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const& leases,
            ::uwvm2::uwvm::runtime::storage::gc_function_type_match_callback callback) const noexcept
        {
            return ::std::make_shared<::uwvm2::uwvm::runtime::storage::gc_object_store>(section, leases, callback);
        }
        template<initialization_purpose Purpose>
        inline ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store>
        initialization_context<Purpose>::make_store(
            ::uwvm2::uwvm::runtime::storage::gc_type::recursive_type_section const& section,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const& leases,
            ::uwvm2::uwvm::runtime::storage::gc_function_type_match_callback callback) const noexcept(Purpose == initialization_purpose::ordinary)
        {
            if constexpr(Purpose == initialization_purpose::ordinary)
            { return ::std::make_shared<::uwvm2::uwvm::runtime::storage::gc_object_store>(section, leases, callback); }
            else
            {
                if(restoration_ == nullptr) { ::fast_io::fast_terminate(); }
                auto result{restoration_->create_store(section, leases)};
#ifdef UWVM_CPP_EXCEPTIONS
                // PRIVATE unpublished preparation can fail its real budget or
                // allocation. Propagate before the shared ordinary initializer's
                // fatal fallback; the sole world owner catches and discards all
                // partial native resources while the original instance survives.
                if(!result) { throw ::std::bad_alloc{}; }
#endif
                return result;
            }
        }
    }
}

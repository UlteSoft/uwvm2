// Embedded-image and actual-position management APIs. No DWARF/LLVM-debug-info
// dependency enters the runtime; run/controller parse the owning copied image.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    llvm_jit_debug_source_binding_owner llvm_jit_debug_source_binding::canonical_locked(
        llvm_jit_debug_source_binding_owner const& supplied) noexcept
    {
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        // Caller already owns the publication guard. A private EH generation
        // cannot be attached until a future genuine drain/rebuild supports it.
        if(runtime_native_eh_private_leaf_management_blocked()) { return {}; }
#endif
        if(!supplied) { return {}; }
        for(auto const& rec : g_runtime.modules)
        {
            auto const& publication{rec.llvm_jit_full_publication};
            if(!publication || !publication->engine || !publication->context || publication->plan) { continue; }
            auto const& owner{publication->debug_source_binding};
            // [runtime-owned binding][comparison-only supplied pointer]
            // [safe                 ][never dereferenced             ]
            //  ^^ equal object address AND control block must precede every read.
            if(owner && owner.get() == supplied.get() && !owner.owner_before(supplied) && !supplied.owner_before(owner))
            { return owner; }
        }
        return {};
    }
    bool llvm_jit_debug_source_binding::position_locked(llvm_jit_debug_source_binding_owner const& supplied,
        ::uwvm2::utils::thread::cooperative_pause_location actual, llvm_jit_debug_source_position& out) noexcept
    {
        out = {};
        auto const binding{canonical_locked(supplied)};
        if(!binding || !g_runtime.compiled_all.load(::std::memory_order_acquire) ||
           actual.code_generation != current_runtime_generation() || binding->runtime_epoch_ != actual.code_generation ||
           binding->module_id_ != actual.code_unit || binding->module_id_ >= g_runtime.modules.size()) { return false; }
        auto const& rec{g_runtime.modules.index_unchecked(binding->module_id_)};
        auto const& publication{rec.llvm_jit_full_publication};
        auto const source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
        if(!rec.llvm_jit_ready || !publication || !publication->engine || !publication->context || publication->plan ||
           publication.get() != binding->publication_ || publication->debug_source_runtime_epoch != actual.code_generation ||
           !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
           !source->initialized_from_actual_state() || source.get() != binding->source_.get() ||
           source.owner_before(binding->source_) || binding->source_.owner_before(source) ||
           publication->source.get() != source.get() || publication->source.owner_before(source) || source.owner_before(publication->source) ||
           rec.llvm_jit_debug_source_fused_epoch != actual.code_generation ||
           source->actual_validated_file(binding->module_id_, actual.code_generation, rec.runtime_module) == nullptr)
        { return false; }
        auto const imports{rec.runtime_module->imported_function_vec_storage.size()};
        if(actual.function < imports) { return false; }
        auto const local{actual.function - imports};
        if(local >= binding->image_.functions.size() || local >= rec.llvm_jit_debug_full_safe_points.size() ||
           local >= rec.llvm_jit_debug_full_entry_generations.size()) { return false; }
        auto const& original{binding->image_.functions[static_cast<::std::size_t>(local)]};
        auto const& point{rec.llvm_jit_debug_full_safe_points[static_cast<::std::size_t>(local)]};
        auto const generation{rec.llvm_jit_debug_full_entry_generations[static_cast<::std::size_t>(local)]};
        if(original.function != actual.function || original.function_generation != generation || point.function_generation != generation ||
           actual.offset >= original.expression_size || !point.contains(actual.offset) ||
           original.expression_begin > binding->image_.code_section_content_size ||
           actual.offset > binding->image_.code_section_content_size - original.expression_begin) { return false; }
        out = {actual.code_unit, actual.function, original.expression_begin + actual.offset, actual.code_generation, generation};
        return true;
    }
#endif
    extern "C++" llvm_jit_debug_source_binding_owner llvm_jit_debug_bind_source_host_api(::std::size_t module_id) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return {}; }
        try
        {
            // A real lease excludes drain/reset of the code and registry while
            // this cold image is copied. It creates no guest participant/frame.
            // Real cold native reader excludes the original managed sweep.
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
            if(!gc_reader) { return {}; }
            auto lease{g_runtime.execution_domain.try_enter()};
            if(!lease) { return {}; }
            runtime_state_publication_guard lock{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            if(runtime_native_eh_private_leaf_management_blocked()) { return {}; }
#endif
            if(!g_runtime.debug_pause_control || !g_runtime.compiled_all.load(::std::memory_order_acquire) ||
               module_id >= g_runtime.modules.size()) { return {}; }
            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto& publication{rec.llvm_jit_full_publication};
            auto const source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            auto const epoch{current_runtime_generation()};
            if(!rec.llvm_jit_ready || !publication || !publication->engine || !publication->context || publication->plan ||
               !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
               !source->initialized_from_actual_state() ||
               source->actual_validated_file(module_id, epoch, rec.runtime_module) == nullptr || publication->source.get() != source.get() ||
               publication->source.owner_before(source) || source.owner_before(publication->source) ||
               publication->debug_source_runtime_epoch != epoch ||
               rec.llvm_jit_debug_source_fused_epoch != epoch) { return {}; }
            if(publication->debug_source_binding) { return publication->debug_source_binding; }
            auto const& functions{rec.runtime_module->local_defined_function_vec_storage};
            auto const imports{rec.runtime_module->imported_function_vec_storage.size()};
            if(functions.size() > 65536u || functions.size() != rec.llvm_jit_debug_full_entry_generations.size() ||
               functions.size() != rec.llvm_jit_debug_full_safe_points.size()) { return {}; }
            ::std::shared_ptr<llvm_jit_debug_source_binding> candidate{new llvm_jit_debug_source_binding};
            // [canonical source owner][live unique code publication] owner_end
            // [safe                 ][safe                       ] the genuine
            //  ^^ lease/publication lock retains the comparison-only borrow;
            // no executable pointer is dereferenced or exposed by this binding.
            candidate->source_ = source; candidate->publication_ = publication.get();
            candidate->module_id_ = module_id; candidate->runtime_epoch_ = epoch;
            candidate->image_.runtime_epoch = epoch; // display label; the private binding remains authority.
            auto const file_owner{source->actual_validated_file(module_id, epoch, rec.runtime_module)};
            if(file_owner == nullptr || !file_owner->has_owned_source_image() || file_owner->binfmt_ver != 1u) { return {}; }
            // Actual canonical source owns this FINAL per-member file, name,
            // immutable bytes and parser context; outer lease/publication pin it.
            auto const& file{*file_owner};
            auto const& module{file.wasm_module_storage.wasm_binfmt_ver1_storage};
            bool valid{true};
            [&]<typename... Fs>(::uwvm2::utils::container::tuple<Fs...>)
            {
                using code_section_t = ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>;
                using custom_section_t = ::uwvm2::parser::wasm::standard::wasm1::features::custom_section_storage_t;
                auto const& code{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<code_section_t>(module.sections)};
                auto const& custom{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<custom_section_t>(module.sections)};
                auto const begin{reinterpret_cast<::std::uintptr_t>(module.module_span.module_begin)};
                auto const end{reinterpret_cast<::std::uintptr_t>(module.module_span.module_end)};
                auto const code_begin{reinterpret_cast<::std::uintptr_t>(code.sec_span.sec_begin)};
                auto const code_end{reinterpret_cast<::std::uintptr_t>(code.sec_span.sec_end)};
                if(begin == 0u || end < begin || code_begin < begin || code_end < code_begin || code_end > end)
                { valid = false; return; }
                candidate->image_.code_section_content_size = code_end - code_begin;
                ::std::size_t copied{};
                for(auto const& entry : custom.customs)
                {
                    // [actual source-owned parsed custom name] name_end
                    // [safe                                 ] bounded before
                    //  ^^ copying this borrow or inspecting any name byte.
                    auto const name{entry.custom_name};
                    auto const name_begin{reinterpret_cast<::std::uintptr_t>(name.data())};
                    if(name_begin < begin || name_begin > end || name.size() > end - name_begin) { valid = false; return; }
                    bool const debug_name{name.size() >= 7u && name[0u] == u8'.' && name[1u] == u8'd' && name[2u] == u8'e' &&
                        name[3u] == u8'b' && name[4u] == u8'u' && name[5u] == u8'g' && name[6u] == u8'_'};
                    if(!debug_name && name != u8"external_debug_info" && name != u8".gnu_debugaltlink" && name != u8".gnu_debuglink") { continue; }
                    auto const payload_begin{reinterpret_cast<::std::uintptr_t>(entry.custom_begin)};
                    auto const payload_end{reinterpret_cast<::std::uintptr_t>(entry.sec_span.sec_end)};
                    if(name.size() > 64u || candidate->image_.sections.size() == 64u || payload_begin < begin ||
                       payload_end < payload_begin || payload_end > end || payload_end - payload_begin > 1024u * 1024u ||
                       payload_end - payload_begin > 8u * 1024u * 1024u - copied) { valid = false; return; }
                    llvm_jit_debug_source_section saved{};
                    // [canonical source-owned name/payload ...] checked end
                    // [safe                                  ] one-past
                    //  ^^ each new borrow is immediately copied; no file/host address escapes.
                    saved.name.assign(reinterpret_cast<char const*>(name.data()), name.size());
                    saved.payload.assign(reinterpret_cast<::std::byte const*>(payload_begin),
                                         reinterpret_cast<::std::byte const*>(payload_end));
                    copied += saved.payload.size(); candidate->image_.sections.push_back(::std::move(saved));
                }
                candidate->image_.functions.reserve(functions.size());
                ::std::size_t expression_bytes_copied{}, instruction_bitmap_bytes_copied{};
                for(::std::size_t local{}; local != functions.size(); ++local)
                {
                    auto const& point{rec.llvm_jit_debug_full_safe_points[local]};
                    auto const* body{functions.index_unchecked(local).wasm_code_ptr};
                    if(body == nullptr || local > (::std::numeric_limits<::std::uint_least64_t>::max)() - imports ||
                       rec.llvm_jit_debug_full_entry_generations[local] != 1u || point.function_generation != 1u)
                    { valid = false; return; }
                    auto const expression{reinterpret_cast<::std::uintptr_t>(body->body.expr_begin)};
                    auto const expression_end{reinterpret_cast<::std::uintptr_t>(body->body.code_end)};
                    if(expression < code_begin || expression_end <= expression || expression_end > code_end ||
                       point.expression_size != expression_end - expression) { valid = false; return; }
                    // [actual owned Code section][locals][expression ... end]
                    // [safe                     ][safe  ][safe               ] one-past
                    //  ^^ only checked integer offsets are copied, not parser pointers.
                    llvm_jit_debug_source_function saved{imports + local, expression - code_begin, expression_end - expression, 1u};
                    auto const bytes{expression_end - expression};
                    constexpr ::std::size_t max_expression_bytes{65536u}, max_module_expression_bytes{8u * 1024u * 1024u};
                    if(bytes <= max_expression_bytes && bytes <= max_module_expression_bytes - expression_bytes_copied)
                    {
                        // [canonical source Code ... expression bytes ...] code_end
                        // [safe                                          ] unsafe (one-past)
                        //                             ^^ expression>=code_begin and
                        // expression_end<=code_end proved BEFORE either borrowed endpoint
                        // is formed; source lease/publication lock retains the entire range.
                        auto const* first{reinterpret_cast<::std::byte const*>(expression)};
                        auto const* last{reinterpret_cast<::std::byte const*>(expression_end)};
                        saved.expression_bytes.assign(first, last);
                        expression_bytes_copied += static_cast<::std::size_t>(bytes);
                        // Copy only the actual emitted instruction bitmap while
                        // the SAME publication/source lease is retained. A coarse
                        // entry/loop bitmap cannot prove a static local unchanged.
                        auto const bitmap_bytes{bytes/8u + (bytes%8u != 0u)};
                        if(g_runtime.debug_granularity == llvm_jit_debug_safe_point_granularity::instruction &&
                           point.bits != nullptr && point.byte_count == bitmap_bytes &&
                           bitmap_bytes <= 1024u*1024u - instruction_bitmap_bytes_copied)
                        {
                            saved.instruction_safe_point_bits.assign(point.bits,point.bits+point.byte_count);
                            saved.instruction_safe_points_complete = true;
                            instruction_bitmap_bytes_copied += static_cast<::std::size_t>(bitmap_bytes);
                        }
                    }
                    // Large bodies preserve source stepping/metadata. Only their
                    // optional opcode category inspection is reported unavailable.
                    candidate->image_.functions.push_back(::std::move(saved));
                }
            }(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
            if(!valid) { return {}; }
            publication->debug_source_binding = candidate; // publish only the complete bounded private copy.
            return candidate;
        }
        catch(...) { return {}; }
#else
        (void)module_id; return {};
#endif
    }
    extern "C++" bool llvm_jit_debug_copy_source_image_host_api(llvm_jit_debug_source_binding_owner const& binding,
        llvm_jit_debug_source_image& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!binding || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            // Real cold native reader excludes the original managed sweep.
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
            if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            runtime_state_publication_guard lock{};
            auto const canonical{llvm_jit_debug_source_binding::canonical_locked(binding)};
            if(!canonical) { return false; }
            if(canonical->runtime_epoch_ != current_runtime_generation() || canonical->module_id_ >= g_runtime.modules.size()) { return false; }
            auto const& rec{g_runtime.modules.index_unchecked(canonical->module_id_)};
            auto const& publication{rec.llvm_jit_full_publication};
            if(!rec.llvm_jit_ready || !publication || publication.get() != canonical->publication_ ||
               publication->debug_source_binding.get() != binding.get() ||
               publication->debug_source_binding.owner_before(binding) || binding.owner_before(publication->debug_source_binding) ||
               publication->debug_source_runtime_epoch != canonical->runtime_epoch_ ||
               !canonical->source_ || canonical->source_->actual_full_validation_epoch() != canonical->runtime_epoch_) { return false; }
            out = canonical->image_; return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)binding; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_source_position_host_api(llvm_jit_debug_source_binding_owner const& binding,
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const& control,
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::uint_least64_t participant, llvm_jit_debug_source_position& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!binding || !control || participant == 0u || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        // A supplied shared_ptr can alias an arbitrary address. Resolve the
        // runtime's control owner by comparison under a short publication guard
        // BEFORE any domain call. Do not carry that guard into the domain lock.
        // Real cold native reader excludes the original managed sweep.
        auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
        if(!gc_reader) { return false; }
        auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> canonical_control{};
        {
            runtime_state_publication_guard lock{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            if(runtime_native_eh_private_leaf_management_blocked()) { return false; }
#endif
            if(!g_runtime.debug_pause_control || g_runtime.debug_pause_control.get() != control.get() ||
               g_runtime.debug_pause_control.owner_before(control) || control.owner_before(g_runtime.debug_pause_control)) { return false; }
            canonical_control = g_runtime.debug_pause_control;
        }
        // Lease -> ONE domain guard -> publication guard. No nested stopped
        // query, LLVM, loader, Wasm execution or frame/native-address read.
        bool valid{};
        auto const stopped{canonical_control->with_stopped_participant(ticket, participant, [&](auto const actual)
        {
            runtime_state_publication_guard lock{};
            if(g_runtime.debug_pause_control.get() != canonical_control.get() ||
               g_runtime.debug_pause_control.owner_before(canonical_control) || canonical_control.owner_before(g_runtime.debug_pause_control)) { return; }
            valid = llvm_jit_debug_source_binding::position_locked(binding, actual, out);
        })};
        if(!stopped || !valid) { out = {}; return false; }
        return true;
#else
        (void)binding; (void)control; (void)ticket; (void)participant; return false;
#endif
    }

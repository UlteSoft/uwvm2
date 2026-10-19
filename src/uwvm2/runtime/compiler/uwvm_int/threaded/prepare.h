#pragma once
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    inline status engine::prepare(::uwvm2::validation::error::code_validation_error_impl& error)
    {
        // This r2 is a real finite backend slice, NOT a complete pthread/ROS
        // publication. Its actual source-shape restrictions are explicit.
        if(world_->module_count()!=1uz || limits_.values<2uz) { return status::unsupported_profile; }
        static_cast<void>(error);
        validated_code_=world_->validated_code();
        if(!validated_code_ || !validated_code_->covers(*world_)) { return status::invalid_world; }
        ::std::size_t source_bytes{},code_bytes{},functions{},storage_bytes{sizeof(engine)};
        auto charge{[](::std::size_t count,::std::size_t width,::std::size_t& used,::std::size_t limit) noexcept
        { if(used>limit || count>(limit-used)/width) { return false; }used+=count*width;return true; }};
        if(!charge(limits_.values,sizeof(::std::uint64_t),storage_bytes,limits_.storage_bytes) ||
            !charge(limits_.calls,sizeof(activation),storage_bytes,limits_.storage_bytes) ||
            !charge(limits_.controls,sizeof(control),storage_bytes,limits_.storage_bytes) ||
            !charge(limits_.host_bytes,2uz,storage_bytes,limits_.storage_bytes) ||
            !charge(limits_.exception_objects,sizeof(exception_record),storage_bytes,limits_.storage_bytes) ||
            !charge(limits_.exception_values,sizeof(::std::uint64_t),storage_bytes,limits_.storage_bytes) ||
            !charge(world_->module_count(),sizeof(bound_module),storage_bytes,limits_.storage_bytes) ||
            !charge(limits_.controls,sizeof(::std::size_t),code_bytes,limits_.instruction_bytes))
        { return status::resource_limit; }
        // Charge one lowering nesting scratch vector at its worst finite
        // capacity throughout preparation; execution never allocates it.
        modules_.resize(world_->module_count());
        for(::std::size_t m{};m<modules_.size();++m)
        {
            auto const* module{world_->module(m)};if(!module || !world_->file(m) || !world_->feature_parameter(m)) { return status::invalid_world; }
            if(module->type_section_storage.requires_simd || module->table_declarations_require_gc ||
                module->global_declarations_require_gc || module->element_declarations_require_gc ||
                !module->imported_table_vec_storage.empty() || module->table_declarations_require_table64)
            { return status::unsupported_profile; }
            auto& bound{modules_[m]};bound.program.instance=module;bound.memory=shared_memory(*module);
            auto const* shared{world_->shared_memory(0)};
            if(!bound.memory || !shared || bound.memory!=::std::addressof(shared->memory)) { return status::unsupported_profile; }
            // These are the ACTUAL initialized effective limits, including
            // explicit module policy. Do not re-intersect declared limits after
            // overrides have replaced them. The address-domain ceiling and
            // group reservation remain separate finite caps.
            auto const& effective{module->imported_memory_vec_storage.empty() ?
                module->local_defined_memory_vec_storage.index_unchecked(0).effective_limits :
                module->imported_memory_vec_storage.index_unchecked(0).effective_limits};
            auto const pages{effective.present_max ? (::std::min)(static_cast<::std::uint64_t>(effective.max),::std::uint64_t{65536u}) : ::std::uint64_t{65536u}};
            if(pages>(static_cast<::std::uint64_t>(SIZE_MAX)>>16u)) { return status::unsupported_profile; }
            bound.maximum_memory_bytes=(::std::min)(static_cast<::std::size_t>(pages<<16u),world_->admission().limits().max_shared_memory_bytes);
            if(!bound.maximum_memory_bytes) { return status::resource_limit; }
            auto const* retained_file{world_->file(m)};
            auto const source_begin{reinterpret_cast<::std::uintptr_t>(retained_file->source_cbegin())};
            auto const source_end{reinterpret_cast<::std::uintptr_t>(retained_file->source_cend())};
            for(auto const& segment:module->local_defined_data_vec_storage)
            {
                auto const& data{segment.data};
                if(data.byte_begin==data.byte_end) { continue; }
                auto const begin{reinterpret_cast<::std::uintptr_t>(data.byte_begin)},end{reinterpret_cast<::std::uintptr_t>(data.byte_end)};
                if(begin<source_begin || end<begin || end>source_end) { return status::invalid_world; }
            }
            auto const count{module->imported_function_vec_storage.size()+module->local_defined_function_vec_storage.size()};
            if(count>limits_.functions-functions) { return status::resource_limit; } functions+=count;
            if(!charge(count,sizeof(target),code_bytes,limits_.instruction_bytes) ||
                !charge(module->local_defined_function_vec_storage.size(),sizeof(function_program),code_bytes,limits_.instruction_bytes))
            { return status::resource_limit; }
            bound.targets.resize(count);bound.program.functions.resize(module->local_defined_function_vec_storage.size());
            for(::std::size_t f{};f<count;++f)
            {
                auto const* type{function_type(m,f)};if(!detail::executable_signature(type)) { return status::unsupported_profile; }
                auto const params{static_cast<::std::size_t>(type->parameter.end-type->parameter.begin)};
                auto const results{static_cast<::std::size_t>(type->result.end-type->result.begin)};
                if(params>limits_.values || results>limits_.values || params>limits_.host_bytes/8uz || results>limits_.host_bytes/8uz)
                { return status::resource_limit; }
            }
            // Refuse recursive/nominal/subtyped definitions that the finite
            // finite signature matcher below does not implement. Defaults do
            // not participate: only actual retained type declarations do.
            auto const* recursive{module->type_section_storage.core3_recursive_types_ptr};
            if(recursive)
            {
                for(::std::size_t i{};i<module->type_section_storage.type_section_count;++i)
                { if(!::uwvm2::validation::standard::wasm3::core3_is_plain_function_definition(recursive,i)) { return status::unsupported_profile; } }
            }
            auto const& types{module->type_section_storage};
            bool const rich{types.owned_signature_begin && types.owned_signature_end &&
                static_cast<::std::size_t>(types.owned_signature_end-types.owned_signature_begin)==types.type_section_count};
            for(::std::size_t i{};i<types.type_section_count;++i)
            {
                if(rich)
                {
                    auto const& signature{types.owned_signature_begin[i]};
                    for(auto const& t:signature.parameters) { if(!detail::value_type_supported(t)) { return status::unsupported_profile; } }
                    for(auto const& t:signature.results) { if(!detail::value_type_supported(t)) { return status::unsupported_profile; } }
                }
                else
                {
                    auto const& signature{types.type_section_begin[i]};
                    for(auto p{signature.parameter.begin};p!=signature.parameter.end;++p) { if(!detail::numeric_type(*p)) { return status::unsupported_profile; } }
                    for(auto p{signature.result.begin};p!=signature.result.end;++p) { if(!detail::numeric_type(*p)) { return status::unsupported_profile; } }
                }
            }
            for(auto const& local: module->local_defined_function_vec_storage)
            {
                if(!local.wasm_code_ptr) { return status::invalid_world; }
                auto const& body{local.wasm_code_ptr->body};
                auto const begin{reinterpret_cast<::std::uintptr_t>(body.expr_begin)},end{reinterpret_cast<::std::uintptr_t>(body.code_end)};
                auto const source_begin{reinterpret_cast<::std::uintptr_t>(world_->file(m)->source_cbegin())};
                auto const source_end{reinterpret_cast<::std::uintptr_t>(world_->file(m)->source_cend())};
                if(begin<source_begin || end<begin || end>source_end) { return status::invalid_world; }
                if(end-begin>limits_.source_bytes-source_bytes) { return status::resource_limit; }
                source_bytes+=end-begin;
                ::std::size_t locals{static_cast<::std::size_t>(local.function_type_ptr->parameter.end-local.function_type_ptr->parameter.begin)};
                for(auto const& run: local.wasm_code_ptr->locals)
                {
                    if(!(run.has_core_type ? detail::value_type_supported(run.core_type) : detail::numeric_type(run.type)) || run.count>limits_.values-locals) { return status::unsupported_profile; }
                    locals+=run.count;
                }
            }
            // No repeated fatal-allocating validator. Only Other's privately
            // minted actual-source/current-validation/world-binding certificate
            // authorizes this; checked bounded lowering remains mandatory.
            auto const global_count{module->imported_global_vec_storage.size()+module->local_defined_global_vec_storage.size()};
            if(global_count>limits_.values || !charge(global_count,sizeof(global_type*),storage_bytes,limits_.storage_bytes))
            { return status::resource_limit; }bound.globals.reserve(global_count);
            for(::std::size_t i{};i<global_count;++i)
            {
                auto* g{resolve_global(*module,i)};if(!g) { return status::unsupported_profile; }
                using kind=::uwvm2::object::global::global_type;
                if((g->kind!=kind::wasm_i32 && g->kind!=kind::wasm_i64 && g->kind!=kind::wasm_f32 && g->kind!=kind::wasm_f64) ||
                    (i<module->imported_global_vec_storage.size() && g->is_mutable)) { return status::unsupported_profile; }
                bound.globals.push_back(g);
            }
            auto const imported_tags{module->imported_tag_vec_storage.size()};
            auto const tag_count{imported_tags+module->local_defined_tag_vec_storage.size()};
            if(tag_count>limits_.functions || !charge(tag_count,sizeof(storage::local_defined_tag_storage_t*),storage_bytes,limits_.storage_bytes))
            { return status::resource_limit; }bound.tags.reserve(tag_count);
            for(::std::size_t i{};i<tag_count;++i)
            {
                auto const* tag{i<imported_tags ? module->imported_tag_vec_storage.index_unchecked(i).resolved_tag :
                    ::std::addressof(module->local_defined_tag_vec_storage.index_unchecked(i-imported_tags))};
                if(!tag || !tag->function_type_ptr || !tag->exception_identity ||
                    tag->function_type_ptr->result.begin!=tag->function_type_ptr->result.end) { return status::unsupported_profile; }
                auto const n{static_cast<::std::size_t>(tag->function_type_ptr->parameter.end-tag->function_type_ptr->parameter.begin)};
                if(n>limits_.exception_values) { return status::resource_limit; }bound.tags.push_back(tag);
            }
            for(::std::size_t f{};f<bound.program.functions.size();++f)
            {
                auto const result{detail::lower(*module,module->local_defined_function_vec_storage.index_unchecked(f),
                    bound.program.functions[f],limits_,code_bytes,source_begin)};
                if(result!=status::ok) { return result; }
                for(auto const& ins: bound.program.functions[f].instructions)
                {
                    if((ins.opcode==0x3fu || ins.opcode==0x40u) && ins.immediate!=0u) { return status::unsupported_profile; }
                    if((ins.opcode==0x308u || ins.opcode==0x309u) && ins.immediate>=module->local_defined_data_vec_storage.size())
                    { return status::validation_failed; }
                    if((ins.opcode==0x308u && ins.target!=0u) ||
                        (ins.opcode==0x30au && (ins.immediate!=0u || ins.target!=0u)) ||
                        (ins.opcode==0x30bu && ins.immediate!=0u)) { return status::unsupported_profile; }
                    if(ins.opcode==0x08u && ins.immediate>=bound.tags.size()) { return status::validation_failed; }
                    if(ins.opcode==0x11u)
                    {
                        if(ins.immediate>=types.type_section_count) { return status::validation_failed; }
                        auto const* t{types.type_section_begin+ins.immediate};
                        if(!detail::equal_signature(t,t)) { return status::unsupported_profile; }
                    }
                    for(auto const& clause:ins.catches)
                    { if(clause.kind<2u && clause.tag>=bound.tags.size()) { return status::validation_failed; } }
                }
            }
        }
        for(::std::size_t m{};m<modules_.size();++m)
        {
            auto& bound{modules_[m]};auto const* module{world_->module(m)};
            for(::std::size_t f{};f<bound.targets.size();++f)
            {
                if(!resolve_target(m,f,bound.targets[f])) { return status::unsupported_profile; }
                if(bound.targets[f].host)
                {
                    auto const* t{bound.targets[f].type};
                    if(!detail::equal_signature(t,t)) { return status::unsupported_profile; }
                }
                if(bound.targets[f].host && threads::admit_import(*world_,m,f)!=threads::import_admission::allowed)
                { return status::host_denied; }
            }
            for(auto const& table: module->local_defined_table_vec_storage)
            {
                if(!table.table_type_ptr || table.elems.size()>limits_.values) { return status::resource_limit; }
                if(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(table.table_type_ptr->reftype)!=value_type::funcref)
                { return status::unsupported_profile; }
                for(auto const& slot: table.elems)
                {
                    if(slot.type==storage::local_defined_table_elem_storage_type_t::func_ref_imported && !slot.storage.imported_ptr) { continue; }
                    if(slot.type!=storage::local_defined_table_elem_storage_type_t::func_ref_defined) { return status::unsupported_profile; }
                    if(slot.storage.defined_ptr)
                    { target t{};if(!defined_target(slot.storage.defined_ptr,t)) { return status::unsupported_profile; } }
                }
            }
        }
        auto const m{world_->entry_module_index()},f{world_->thread_start_function_index()};
        if(m>=modules_.size() || f>=modules_[m].targets.size() || modules_[m].targets[f].host) { return status::invalid_world; }
        auto const* t{function_type(m,f)};
        if(!t || t->parameter.end-t->parameter.begin!=2 || t->result.begin!=t->result.end ||
            t->parameter.begin[0]!=value_type::i32 || t->parameter.begin[1]!=value_type::i32) { return status::invalid_world; }
        values_.resize(limits_.values);calls_.resize(limits_.calls);controls_.resize(limits_.controls);
        host_parameters_.resize(limits_.host_bytes);host_results_.resize(limits_.host_bytes);
        exceptions_.resize(limits_.exception_objects);exception_values_.resize(limits_.exception_values);
        return owned_storage_bytes()<=limits_.storage_bytes && owned_code_bytes()<=limits_.instruction_bytes ? status::ok : status::resource_limit;
    }
}

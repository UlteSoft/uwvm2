// Preserve the native ELF macro while using LLVM enumerators in this member fragment.
#pragma push_macro("STT_TLS")
#undef STT_TLS
// ThreadedLlvmChild private cold factory member fragment.
        class bounded_object_stream final:public ::llvm::raw_pwrite_stream
        {
            ::std::vector<char> bytes_;::std::uint64_t position_{};::std::size_t used_{};bool overflow_{};
            void write_impl(char const* data,::std::size_t size) override
            {
                if(position_>bytes_.size()||size>bytes_.size()-static_cast<::std::size_t>(position_))overflow_=true;
                else{::std::memcpy(bytes_.data()+position_,data,size);used_=(::std::max)(used_,static_cast<::std::size_t>(position_)+size);}
                if(size>UINT64_MAX-position_){position_=UINT64_MAX;overflow_=true;}else position_+=size;
            }
            void pwrite_impl(char const* data,::std::size_t size,::std::uint64_t offset) override
            {
                if(offset>used_||size>used_-static_cast<::std::size_t>(offset)){overflow_=true;return;}
                ::std::memcpy(bytes_.data()+offset,data,size);
            }
            [[nodiscard]] ::std::uint64_t current_pos() const override{return position_;}
        public:
            explicit bounded_object_stream(::std::size_t maximum):bytes_(maximum){SetUnbuffered();}
            ~bounded_object_stream() override=default;
            [[nodiscard]] bool valid() const noexcept{return !overflow_&&used_!=0u;}
            [[nodiscard]] ::llvm::StringRef bytes() const noexcept{return {bytes_.data(),used_};}
        };
        [[nodiscard]] bool build_local_type_ids()
        {
            // Expensive immutable type canonicalization is cold/pre-effect only.
            // Only fixed preallocated IDs survive into the refresh operation.
            if(local_type_ids_.size()!=entries_.size())return false;
            compiled_module_record record{};record.runtime_module=module_;
            record.type_canon_index=build_type_canon_index(*module_);
            for(::std::size_t i{};i<local_type_ids_.size();++i)
            {
                auto const* type=module_->local_defined_function_vec_storage.index_unchecked(i).function_type_ptr;
                auto const id=find_canonical_type_id_for_type_uncached(record,*module_,type);
                if(id==invalid_llvm_jit_encoded_type_id())return false;
                local_type_ids_[i]=id;
            }
            return true;
        }
        [[nodiscard]] ::std::size_t actual_table_local(
            ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const* function) const noexcept
        {
            for(::std::size_t i{};i<entries_.size();++i)
                if(function==::std::addressof(module_->local_defined_function_vec_storage.index_unchecked(i)))return i;
            return SIZE_MAX;
        }
        [[nodiscard]] bool validate_table_targets_noalloc() const noexcept
        {
            namespace st=::uwvm2::uwvm::runtime::storage;
            if(module_==nullptr||!module_->imported_table_vec_storage.empty()||
               table_targets_.size()!=module_->local_defined_table_vec_storage.size()||
               table_views_.size()!=table_targets_.size()||local_type_ids_.size()!=entries_.size())return false;
            for(::std::size_t t{};t<table_targets_.size();++t)
            {
                auto const& table=module_->local_defined_table_vec_storage.index_unchecked(t);
                if(table_targets_[t].size()!=table.elems.size()||table.owner_module_rt_ptr!=module_||
                   table.table_type_ptr==nullptr||st::runtime_table_family(table)!=st::runtime_table_reference_family::function)return false;
                for(auto const& element:table.elems)
                {
                    if(element.type==st::local_defined_table_elem_storage_type_t::func_ref_imported&&element.storage.imported_ptr==nullptr)continue;
                    if(element.type!=st::local_defined_table_elem_storage_type_t::func_ref_defined)return false;
                    if(element.storage.defined_ptr==nullptr)continue;
                    auto const local=actual_table_local(element.storage.defined_ptr);
                    if(local==SIZE_MAX||entries_[local].raw==0u||entries_[local].typed==0u||
                       local_type_ids_[local]==invalid_llvm_jit_encoded_type_id())return false;
                }
            }
            return true;
        }
        [[nodiscard]] bool refresh_table_targets_noalloc() noexcept
        {
            namespace st=::uwvm2::uwvm::runtime::storage;
            if(!validate_table_targets_noalloc())return false;
            for(::std::size_t t{};t<table_targets_.size();++t)
            {
                auto const& table=module_->local_defined_table_vec_storage.index_unchecked(t);
                auto& targets=table_targets_[t];
                for(::std::size_t slot{};slot<targets.size();++slot)
                {
                    auto const& element=table.elems.index_unchecked(slot);auto& target=targets[slot];
                    target={};target.encoded_type_id=invalid_llvm_jit_encoded_type_id();
                    if(element.type==st::local_defined_table_elem_storage_type_t::func_ref_imported||element.storage.defined_ptr==nullptr)continue;
                    auto const local=actual_table_local(element.storage.defined_ptr);
                    target.entry_address=entries_[local].raw;target.typed_entry_address=entries_[local].typed;
                    target.encoded_type_id=local_type_ids_[local];
                }
                table_views_[t]={targets.empty()?0u:reinterpret_cast<::std::uintptr_t>(targets.data()),targets.size()};
            }
            bound_tables_=true;return true;
        }
        [[nodiscard]] bool check_object(::llvm::object::ObjectFile const& object,::std::size_t& charge,
            ::std::vector<::std::string>& definitions)
        {
            // A finite, deliberately conservative x86-64 ELF loader charge:
            // real section bytes, max alignment per section, relocation stub/GOT
            // allowance and three native page groups. No null allocator is used
            // to pretend RuntimeDyld's fatal allocation path is recoverable.
            if(!object.isELF()||object.getArch()!=::llvm::Triple::x86_64||!object.isLittleEndian()||object.getBytesInAddress()!=8u)return false;
            charge=3u*65536u;::std::size_t sections{},relocations{},symbols{},function_symbols{};
            auto add=[&](::std::size_t value){if(charge>code_limit_||value>code_limit_-charge)return false;charge+=value;return true;};
            for(auto const& section:object.sections())
            {
                if(++sections>4096u||section.getAlignment().value()>65536u)return false;
                auto name=section.getName();if(!name){::llvm::consumeError(name.takeError());return false;}
                auto const size=section.getSize();if(size>code_limit_||!add(static_cast<::std::size_t>(size))||!add(2u*65536u))return false;
                for(auto const& relocation:section.relocations())
                {static_cast<void>(relocation);if(++relocations>65536u||!add(512u))return false;}
            }
            for(auto const& symbol:object.symbols())
            {
                if(++symbols>262144u)return false;
                auto flags=symbol.getFlags();auto name=symbol.getName();
                if(!flags){::llvm::consumeError(flags.takeError());return false;}
                if(!name){::llvm::consumeError(name.takeError());return false;}
                if((*flags&::llvm::object::SymbolRef::SF_Common)||::llvm::object::ELFSymbolRef{symbol}.getELFType()==::llvm::ELF::STT_TLS)return false;
                if(*flags&::llvm::object::SymbolRef::SF_Undefined)
                {
                    if(name->empty())continue;
                    bool mapped{};for(auto const& binding:externals_)if(binding.name==*name)mapped=true;
                    // Object-lowered native libcalls are narrow, audited finite
                    // memory operations, never dlsym of a Guest import. Huge
                    // intrinsic work must still fit admitted linear memory.
                    if(!mapped)
                    {
                        ::std::uintptr_t pointer{};
                        if(*name=="memcpy")pointer=address(static_cast<void*(*)(void*,void const*,::std::size_t)>(&::std::memcpy));
                        else if(*name=="memmove")pointer=address(static_cast<void*(*)(void*,void const*,::std::size_t)>(&::std::memmove));
                        else if(*name=="memset")pointer=address(static_cast<void*(*)(void*,int,::std::size_t)>(&::std::memset));
                        if(pointer==0u||externals_.size()>=256u)return false;
                        externals_.push_back({name->str(),pointer});
                    }
                }
                else if(!name->empty())
                {
                    definitions.push_back(name->str());
                    auto type=symbol.getType();if(!type){::llvm::consumeError(type.takeError());return false;}
                    if(*type==::llvm::object::SymbolRef::ST_Function)++function_symbols;
                }
            }
            if(!check_static_stack_object(object))return false;
            return charge<=code_limit_;
        }
        [[nodiscard]] static preparation_result build(world_type::owner world,
            ::uwvm2::runtime::wasm_threads::ThreadedGroup::CompilerLease const& compiler,
            ::std::size_t backend_storage_budget) noexcept
        {
            namespace compile=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
            namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            using status=preparation_status;
            if(!world||active_!=nullptr||!compiler.owns_current_native_thread()||
               ::std::addressof(compiler.admission())!=::std::addressof(world->admission())||
               compiler.cancellation().stop_requested())return {};
            auto actual_code=world->validated_code();
            if(!actual_code||!actual_code->covers(*world)||actual_code->file()!=world->file(0u))return {};
            if(!world->admission().owns_actual_generation()||world->module_count()!=1u||world->entry_module_index()!=0u||
               world->module(0u)==nullptr||world->file(0u)==nullptr||world->feature_parameter(0u)==nullptr)return {};
#if !defined(__x86_64__) || __SIZEOF_POINTER__ != 8
            return {{},status::unsupported_shape};
#else
            try
            {
                auto data=::std::unique_ptr<ThreadedLlvmChild>{new ThreadedLlvmChild{::std::move(world)}};
                data->module_=data->world_->module(0u);auto const& module=*data->module_;
                auto const& limits=data->world_->admission().limits();
                data->call_limit_=limits.max_call_frames;data->code_limit_=limits.max_child_code_bytes;
                data->guest_native_stack_usable_bytes_=compiler.guest_native_stack_usable_bytes();
                data->compiler_native_stack_usable_bytes_=compiler.native_stack_usable_bytes();
                if(data->guest_native_stack_usable_bytes_==0u||data->compiler_native_stack_usable_bytes_==0u)
                    return {{},status::invalid_world};
                if(data->call_limit_==0u||data->call_limit_>65536u||data->code_limit_<262144u||data->code_limit_>(64u<<20u)||
                   limits.max_child_storage_bytes==0u)return {{},status::budget};
                data->imports_=module.imported_function_vec_storage.size();
                auto const count=module.local_defined_function_vec_storage.size();
                data->start_=data->world_->thread_start_function_index();
                if(count==0u||count>65536u||data->imports_>65536u||count>SIZE_MAX-data->imports_||
                   data->start_<data->imports_||data->start_-data->imports_>=count)return {{},status::unsupported_shape};
                if(module.table_declarations_require_gc||
                   module.global_declarations_require_gc||module.element_declarations_require_gc||
                   module.table_declarations_require_exceptions||module.global_declarations_require_exceptions||
                   module.element_declarations_require_exceptions||module.memory_declarations_require_memory64||
                   module.table_declarations_require_table64||!module.imported_table_vec_storage.empty())return {{},status::unsupported_shape};
                // Real imported-memory aliases must have been normalized to a
                // canonical source-owned defined native memory by World factory.
                using memory_link=::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                for(auto const& imported:module.imported_memory_vec_storage)
                    if(imported.link_kind!=memory_link::defined||imported.target.defined_ptr==nullptr)return {{},status::unsupported_shape};
                auto const memory_count=module.imported_memory_vec_storage.size()+module.local_defined_memory_vec_storage.size();
                if(memory_count>16u)return {{},status::budget};
                data->memories_.reserve(memory_count);
                for(::std::size_t i{};i<memory_count;++i)
                {
                    auto const info=emit::resolve_runtime_memory_access_info(module,static_cast<::std::uint32_t>(i));
                    if(info.memory_p==nullptr||!::uwvm2::runtime::wasm_threads::is_shared(*info.memory_p)||
                       info.local_imported_module_ptr!=nullptr||info.stable_memory_begin==nullptr||
                       info.stable_memory_length_p==nullptr||info.custom_page_size_log2!=16u)return {{},status::unsupported_shape};
                    data->memories_.push_back(info.memory_p);
                }
                for(::std::size_t i{};i<data->imports_;++i)
                    if(::uwvm2::runtime::wasm_threads::admit_import(*data->world_,0u,i)!=
                       ::uwvm2::runtime::wasm_threads::import_admission::allowed)return {{},status::unsupported_shape};
                ::std::size_t bodies{};
                auto const* file=data->world_->file(0u);
                auto const base=reinterpret_cast<::std::uintptr_t>(file->source_cbegin());auto const bytes=file->source_size();
                if(base==0u||bytes==0u)return {{},status::invalid_world};
                for(auto const& local:module.local_defined_function_vec_storage)
                {
                    if(local.function_type_ptr==nullptr||local.wasm_code_ptr==nullptr)return {{},status::invalid_body};
                    auto const& signature=*local.function_type_ptr;
                    auto const parameters=signature.parameter.begin==signature.parameter.end?0u:
                        static_cast<::std::size_t>(signature.parameter.end-signature.parameter.begin);
                    if(parameters>65536u)return {{},status::budget};
                    ::std::size_t locals=parameters;
                    for(auto const& group:local.wasm_code_ptr->locals)
                    {if(group.count>65536u-locals)return {{},status::budget};locals+=group.count;}
                    if(locals>limits.max_value_stack_bytes/16u)return {{},status::budget};
                    auto const begin=reinterpret_cast<::std::uintptr_t>(local.wasm_code_ptr->body.code_begin);
                    auto const expression=reinterpret_cast<::std::uintptr_t>(local.wasm_code_ptr->body.expr_begin);
                    auto const end=reinterpret_cast<::std::uintptr_t>(local.wasm_code_ptr->body.code_end);
                    if(begin<base||expression<begin||end<=expression||end-base>bytes||end-begin>(1u<<20u)||
                       bodies>limits.max_child_storage_bytes||end-begin>limits.max_child_storage_bytes-bodies)return {{},status::budget};
                    bodies+=end-begin;if(bodies>(8u<<20u))return {{},status::budget};
                }
                auto const world_charge=data->world_->owned_storage_bytes();
                if(world_charge>limits.max_child_storage_bytes)return {{},status::budget};
                auto const remaining=(::std::min)(backend_storage_budget,limits.max_child_storage_bytes-world_charge);
                data->storage_limit_=remaining;
                // Monotonic structural PRECHARGE: class + fixed native-manager
                // arrays + per-function packet/entry/name space. Never silently
                // use old request counts or assume zero allocator overhead/RSS.
                constexpr ::std::size_t fixed_charge=sizeof(ThreadedLlvmChild)+sizeof(native_arena)+8192u*64u+
                    256u*(sizeof(external)+512u+sizeof(native_frame_record)+256u);
                if(fixed_charge>remaining||count>(remaining-fixed_charge)/(sizeof(local_type)+sizeof(entries)+sizeof(::std::uint_least32_t)+2u*sizeof(native_frame_record)+768u))
                    return {{},status::budget};
                data->locals_.resize(count);data->entries_.resize(count);data->local_type_ids_.resize(count);data->native_frames_.reserve(count*2u+256u);data->emitted_bridges_.reserve(256u);data->externals_.reserve(256u);
                auto const tables=module.local_defined_table_vec_storage.size();
                if(tables>256u)return {{},status::unsupported_shape};
                auto const structural_function_charge=fixed_charge+count*(sizeof(local_type)+sizeof(entries)+sizeof(::std::uint_least32_t)+2u*sizeof(native_frame_record)+768u);
                auto const table_shape_unit=sizeof(decltype(data->table_targets_)::value_type)+sizeof(decltype(data->table_views_)::value_type);
                if(structural_function_charge>remaining||tables>(remaining-structural_function_charge)/table_shape_unit)
                    return {{},status::budget};
                auto const table_shape_charge=structural_function_charge+tables*table_shape_unit;
                data->table_targets_.resize(tables);data->table_views_.resize(tables);
                ::std::size_t elements{};
                for(::std::size_t i{};i<tables;++i)
                {
                    auto const n=module.local_defined_table_vec_storage.index_unchecked(i).elems.size();
                    if(elements>limits.max_table_elements||n>limits.max_table_elements-elements)return {{},status::budget};
                    elements+=n;
                    if(elements>(remaining-table_shape_charge)/sizeof(target_type))return {{},status::budget};
                    data->table_targets_[i].resize(n);
                }
                if(elements>(SIZE_MAX-table_shape_charge)/sizeof(target_type)||
                   !data->prepare_exception_schemas(table_shape_charge+elements*sizeof(target_type)))return {{},status::budget};
                auto validation=emit::build_runtime_validation_module(module);
                ::uwvm2::validation::error::code_validation_error_impl error{};
                emit::require_runtime_module_declaration_policy(module,*data->world_->feature_parameter(0u),error);
                compile::compile_option options{};options.curr_wasm_id=0u;options.compiler_registry=nullptr; // private cold scope authenticates the exact single module
                options.compilation_mode=compile::llvm_jit_compilation_mode::full;
                options.validator_feature_parameter=data->world_->feature_parameter(0u);options.verify_llvm_jit_ir=true;
                options.emit_call_stack_frames=false;options.emit_unwind_call_stack_frames=false;
                options.emit_precise_gc_root_frames=false;options.emit_debug_safe_points=false;
                compile::llvm_jit_module_storage_t ir{};
                if(!emit::try_prepare_runtime_llvm_jit_module_storage(module,ir,false,nullptr))return {{},status::invalid_body};
                {
                    emit::threaded_host_emission_scope owner{*data->world_,module,*ir.llvm_module,data->emitted_bridges_,
                        reinterpret_cast<::std::uintptr_t>(module.llvm_jit_call_indirect_table_views.data()),
                        reinterpret_cast<::std::uintptr_t>(data->table_views_.data())};
                    for(::std::size_t i{};i<count;++i)
                    {
                        if(compiler.cancellation().stop_requested())return {{},status::invalid_world};
                        data->locals_[i]=emit::compile_all_from_uwvm_local_func(module,validation,options,i,error,::std::addressof(ir));
                        data->locals_[i].compiler_registry=nullptr;
                        if(ir.llvm_module==nullptr||ir.llvm_context_holder==nullptr||ir.first_decline.available())return {{},status::invalid_body};
                        auto const typed=get_runtime_llvm_jit_wasm_function_name(module,data->imports_+i);
                        auto const raw=get_runtime_llvm_jit_wasm_raw_function_name(module,data->imports_+i);
                        if(typed.size()>128u||raw.size()>128u)return {{},status::budget};
                    data->entries_[i].typed_name=emit::get_llvm_string_ref(typed).str();
                        data->entries_[i].raw_name=emit::get_llvm_string_ref(raw).str();
                    }
                    if(!owner.valid()||!emit::finalize_runtime_llvm_jit_module_storage(ir,true))return {{},status::invalid_body};
                }
                if(compiler.cancellation().stop_requested())return {{},status::invalid_world};
                if(!data->bind_emitted_bridges())return {{},status::unsupported_bridge};
                auto& llvm_module=*ir.llvm_module;
                // Pre- and post-pass finite IR quotas. These are NOT a claim of
                // a hard bound on all LLVM optimizer heap allocations.
                auto ir_fits=[&]
                {
                    ::std::size_t instructions{},blocks{};
                    for(auto const& f:llvm_module)for(auto const& b:f)
                    {if(++blocks>262144u||b.size()>1048576u-instructions)return false;instructions+=b.size();}
                    return true;
                };
                if(!ir_fits()||!data->add_control(llvm_module)||!ir_fits()||
                   !data->check_static_stack_ir(llvm_module))return {{},status::budget};
                if(!ensure_llvm_jit_native_target_initialized())return {{},status::native_failure};
                auto const cpu=get_llvm_jit_host_cpu_name_storage();auto const attrs=get_llvm_jit_host_target_attribute_storage();
                ::llvm::SmallVector<::llvm::StringRef,16> refs;append_llvm_jit_host_target_attribute_refs(attrs,refs);
                // Threaded control is an explicit new profile. No reuse of an
                // ordinary/debug cache object, no inherited publication/epoch.
                auto const policy=resolve_runtime_llvm_jit_full_materialize_strategy(::llvm::CodeGenOptLevel::Default);
                ::llvm::EngineBuilder builder;builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(policy.codegen_opt_level)
                    .setMCPU(emit::get_llvm_string_ref(cpu)).setMAttrs(refs);
                auto target=select_runtime_llvm_jit_target(builder,cpu,attrs);if(!target)return {{},status::native_failure};
                target->Options.EmitStackSizeSection=true;
                set_llvm_module_target_triple_from_machine(llvm_module,*target);llvm_module.setDataLayout(target->createDataLayout());
                apply_runtime_llvm_jit_native_target_function_attrs(llvm_module,cpu,cpu,*target);
                if(!optimize_runtime_llvm_jit_module(llvm_module,*target,policy.pipeline,policy.codegen_opt_level,true,false)||
                   !ir_fits()||!data->check_static_stack_ir(llvm_module)||!prove_stop_cycles(llvm_module)||::llvm::verifyModule(llvm_module))return {{},status::native_failure};
                bounded_object_stream output{data->code_limit_};
                {
                    // Destroy codegen passes/MMI while their actual module,
                    // context and target still exist; never retain this cold
                    // pipeline across loader-engine ownership transfer.
                    observed_native_pass_manager native{*data};
                    if(target->addPassesToEmitFile(native,output,nullptr,::llvm::CodeGenFileType::ObjectFile))return {{},status::native_failure};
                    native.run(llvm_module);
                    if(!native.valid())return {{},status::native_failure};
                }
                if(compiler.cancellation().stop_requested())return {{},status::invalid_world};
                if(!output.valid())return {{},status::budget};
                auto buffer=::llvm::MemoryBuffer::getMemBufferCopy(output.bytes(),"uwvm.threaded.child.object");
                if(!buffer)return {{},status::native_failure};
                auto parsed=::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef());
                if(!parsed){::llvm::consumeError(parsed.takeError());return {{},status::native_failure};}
                ::std::vector<::std::string> definitions;
                if(!data->check_object(**parsed,data->native_bytes_,definitions))return {{},status::budget};
                for(auto const& entry:data->entries_)
                {
                    bool raw{},typed{};for(auto const& name:definitions){raw|=name==entry.raw_name;typed|=name==entry.typed_name;}
                    if(!raw||!typed)return {{},status::native_failure};
                }
                // Native object is complete and checked BEFORE an empty loader
                // engine is created. The original IR never gets compiled twice.
                ir.llvm_module.reset();data->emitted_bridges_.clear();
                ir.llvm_context_holder.reset();
                ir.llvm_context_holder=::uwvm2::utils::container::make_delete_owned<::llvm::LLVMContext>();
                if(!ir.llvm_context_holder)return {{},status::native_failure};
                auto empty=::uwvm2::utils::container::make_delete_owned<::llvm::Module>("uwvm.threaded.child.loader",*ir.llvm_context_holder);
                if(!empty)return {{},status::native_failure};
                set_llvm_module_target_triple_from_machine(*empty,*target);empty->setDataLayout(target->createDataLayout());
                ir.llvm_module.reset();
                data->native_arena_=::std::make_unique<native_arena>(data->code_limit_,data->externals_);
                if(!data->native_arena_||!data->native_arena_->valid())return {{},status::native_failure};
                auto* memory_observer=data->native_arena_.get();
                data->native_bytes_=memory_observer->owned_bytes();
                auto memory=::uwvm2::utils::container::make_delete_owned<loader_memory_adapter>(*memory_observer);
                if(!memory)return {{},status::native_failure};
                auto* engine=::llvm::EngineBuilder(llvm_module_owner_t{empty.release()}).setEngineKind(::llvm::EngineKind::JIT)
                    .setOptLevel(policy.codegen_opt_level).setMCPU(emit::get_llvm_string_ref(cpu)).setMAttrs(refs)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory.release()}).create(target.release());
                if(engine==nullptr)return {{},status::native_failure};
                data->context_=::std::move(ir.llvm_context_holder);data->engine_.reset(engine);engine->DisableSymbolSearching();
                for(auto const& mapping:data->externals_)engine->addGlobalMapping(mapping.name,mapping.address);
                data->ranges_=::uwvm2::utils::container::make_delete_owned<details::pending_llvm_jit_code_ranges>(*engine,true);
                if(!data->ranges_)return {{},status::native_failure};data->ranges_->configure_debug_full_capture(true);
                engine->addObjectFile(::llvm::object::OwningBinary<::llvm::object::ObjectFile>{::std::move(*parsed),::std::move(buffer)});
                engine->finalizeObject();
                if(engine->hasError()||memory_observer->has_finalization_failure()||data->ranges_->has_observation_failure())return {{},status::native_failure};
                auto resolve=[&](::std::string const& name)->::std::uintptr_t
                {
                    // Definitions were proved in THIS object, global searching
                    // is disabled, and this listener must own the exact entry.
                    auto const pointer=engine->getFunctionAddress(name);
                    if(pointer==0u)return 0u;
                    auto const code=details::native_function_code_address(static_cast<::std::uintptr_t>(pointer));
                    return data->ranges_->owns_pending_loaded_function_entry(code)?static_cast<::std::uintptr_t>(pointer):0u;
                };
                for(auto& entry:data->entries_)
                {entry.typed=resolve(entry.typed_name);entry.raw=resolve(entry.raw_name);if(!entry.typed||!entry.raw)return {{},status::native_failure};}
                if(!data->build_local_type_ids()||!data->refresh_table_targets_noalloc())return {{},status::unsupported_shape};
                // All object relocations and exact loaded entries are fixed.
                // No lazy function/provider remains. Cold LLVM owns no execution
                // resource: detach observer, drop loader, context and compiler
                // packets BEFORE final actual retained-request accounting.
                data->ranges_.reset();data->engine_.reset();data->context_.reset();
                ::std::vector<local_type>{}.swap(data->locals_);
                ::std::vector<native_bridge>{}.swap(data->emitted_bridges_);
                for(auto& entry:data->entries_)
                {::std::string{}.swap(entry.typed_name);::std::string{}.swap(entry.raw_name);}
                ::std::vector<external>{}.swap(data->externals_);
                if(!data->account_retained_storage())return {{},status::budget};
                return {::std::move(data),status::ok};
            }
            catch(::fast_io::error const&){return {{},status::invalid_body};}
            catch(::std::bad_alloc const&){return {{},status::budget};}
            catch(...){return {{},status::native_failure};}
#endif
        }
#pragma pop_macro("STT_TLS")

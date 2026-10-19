// PRIVATE engine-owned immutable status-EH arena. SOURCE ONLY.
// All allocations are cold/precharged. Execution copies into fixed actual
// buffers and never calls an ordinary runtime/GC exception store.
        friend struct threaded_exception_bridge_api;
        using eh_tag=::uwvm2::uwvm::runtime::storage::local_defined_tag_storage_t;
        using eh_reference=::uwvm2::uwvm::runtime::storage::gc_reference;
        using eh_kind=::uwvm2::object::global::wasm_ref_kind;
        enum class eh_failure : ::std::uint32_t
        { uncaught=0x54454801u,resource=0x54454802u,invalid=0x54454803u };
        struct eh_field{::std::uint8_t width{};bool reference{},nullable{},nonnull{};};
        struct eh_schema{eh_tag const* tag{};::std::size_t first{},count{},bytes{};};
        struct eh_record{eh_tag const* tag{};::std::size_t offset{},bytes{};};
        ::std::vector<eh_schema> eh_schemas_{};
        ::std::vector<eh_field> eh_fields_{};
        ::std::vector<eh_record> eh_records_{};
        ::std::vector<::std::byte> eh_payload_{};
        ::std::size_t eh_record_used_{},eh_payload_used_{},eh_pending_{};
        static void exception_failure(eh_failure value) noexcept
        {fail(static_cast<::std::uint32_t>(value));}
        [[nodiscard]] eh_schema const* exception_schema(::std::size_t tag) const noexcept
        {return tag<eh_schemas_.size()?::std::addressof(eh_schemas_[tag]):nullptr;}
        [[nodiscard]] eh_record const* exception_record(::std::size_t token) const noexcept
        {return token!=0u&&token<=eh_record_used_?::std::addressof(eh_records_[token-1u]):nullptr;}
        [[nodiscard]] ::std::size_t reference_token(eh_reference const& ref) const noexcept
        {
            if(ref.kind!=eh_kind::wasm_exn||ref.storage.ptr==nullptr||eh_record_used_==0u)return 0u;
            auto base=reinterpret_cast<::std::uintptr_t>(eh_records_.data());
            auto value=reinterpret_cast<::std::uintptr_t>(ref.storage.ptr);
            if(value<base||value-base>=eh_record_used_*sizeof(eh_record)||(value-base)%sizeof(eh_record)!=0u)return 0u;
            auto token=(value-base)/sizeof(eh_record)+1u;
            return ref.storage.ptr==::std::addressof(eh_records_[token-1u])?token:0u;
        }
        [[nodiscard]] bool exception_tuple_valid(eh_schema const& schema,::std::byte const* input) const noexcept
        {
            if(schema.bytes!=0u&&input==nullptr)return false;
            ::std::size_t at{};
            for(::std::size_t i{};i<schema.count;++i)
            {
                if((i&255u)==0u&&stopped())return false;
                auto const& field=eh_fields_[schema.first+i];
                if(field.reference)
                {
                    eh_reference ref{};::std::memcpy(&ref,input+at,sizeof(ref));
                    bool null=ref.kind==eh_kind::wasm_null||(ref.kind==eh_kind::wasm_exn&&ref.storage.ptr==nullptr);
                    if(null?!field.nullable:(!field.nonnull||reference_token(ref)==0u))return false;
                }
                at+=field.width;
            }
            return at==schema.bytes;
        }
        [[nodiscard]] bool prepare_exception_schemas(::std::size_t already_charged)
        {
            auto const& types=module_->type_section_storage;
            auto type_count=types.type_section_count;
            bool rich=types.owned_signature_begin!=nullptr&&types.owned_signature_end!=nullptr&&
                types.owned_signature_end>=types.owned_signature_begin&&
                static_cast<::std::size_t>(types.owned_signature_end-types.owned_signature_begin)==type_count;
            auto supported=[](auto const& type) noexcept
            {
                using kind=::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
                using heap=::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
                return type.kind<=kind::v128||(type.kind==kind::reference&&
                    (type.heap.code==static_cast<::std::int64_t>(heap::exn)||type.heap.code==static_cast<::std::int64_t>(heap::noexn)));
            };
            // Rich metadata is the actual retained signature, never inferred
            // from the shared 0x69 carrier of arbitrary aggregate references.
            if(rich)for(::std::size_t i{};i<type_count;++i)
            {
                for(auto const& type:types.owned_signature_begin[i].parameters)if(!supported(type))return false;
                for(auto const& type:types.owned_signature_begin[i].results)if(!supported(type))return false;
            }
            auto imported=module_->imported_tag_vec_storage.size();
            auto count=imported+module_->local_defined_tag_vec_storage.size();
            if(count<imported||count>65536u)return false;
            ::std::size_t fields{},largest{};
            auto get=[&](::std::size_t i)->eh_tag const*
            {return i<imported?module_->imported_tag_vec_storage.index_unchecked(i).resolved_tag:
                ::std::addressof(module_->local_defined_tag_vec_storage.index_unchecked(i-imported));};
            auto type_index=[&](::std::size_t i)->::std::size_t
            {
                if(i<imported)
                {auto p=module_->imported_tag_vec_storage.index_unchecked(i).import_type_ptr;return p==nullptr?SIZE_MAX:p->imports.storage.tag_type_index;}
                return module_->local_defined_tag_vec_storage.index_unchecked(i-imported).type_index;
            };
            auto shape=[&](eh_tag const& tag,::std::size_t index,::std::size_t& bytes)->bool
            {
                bytes=0u;
                if(tag.function_type_ptr==nullptr||!tag.exception_identity||
                   tag.function_type_ptr->result.begin!=tag.function_type_ptr->result.end)return false;
                auto const& params=tag.function_type_ptr->parameter;
                auto n=params.begin==params.end?0u:static_cast<::std::size_t>(params.end-params.begin);
                if(n>65536u)return false;
                if(index>=type_count)return false;
                if(rich&&types.owned_signature_begin[index].parameters.size()!=n)return false;
                for(::std::size_t k{};k<n;++k)
                {
                    auto code=static_cast<unsigned>(params.begin[k]);::std::size_t width{};
                    if(code==0x7fu||code==0x7du)width=4u;
                    else if(code==0x7eu||code==0x7cu)width=8u;
                    else if(code==0x7bu)width=16u;
                    else if(code==0x69u&&rich&&
                        types.owned_signature_begin[index].parameters.index_unchecked(k).kind==::uwvm2::parser::wasm::standard::wasm3::type::value_kind::reference&&
                        supported(types.owned_signature_begin[index].parameters.index_unchecked(k)))width=sizeof(eh_reference);
                    else return false;
                    if(bytes>world_->admission().limits().max_value_stack_bytes||
                       width>world_->admission().limits().max_value_stack_bytes-bytes)return false;
                    bytes+=width;
                }
                return true;
            };
            for(::std::size_t i{};i<count;++i)
            {
                auto tag=get(i);::std::size_t bytes{};if(tag==nullptr||!shape(*tag,type_index(i),bytes))return false;
                auto const& params=tag->function_type_ptr->parameter;
                auto n=params.begin==params.end?0u:static_cast<::std::size_t>(params.end-params.begin);
                if(fields>65536u-n)return false;fields+=n;
                largest=(::std::max)(largest,bytes);
            }
            auto records=(::std::min)(world_->admission().limits().max_call_frames,::std::size_t{65536u});
            auto payload=count==0u?0u:world_->admission().limits().max_value_stack_bytes;
            if(count!=0u&&records==0u)return false;
            ::std::size_t charge=already_charged;
            auto add=[&](::std::size_t n,::std::size_t size)
            {if(n>SIZE_MAX/size||charge>storage_limit_||n*size>storage_limit_-charge)return false;charge+=n*size;return true;};
            if(!add(count,sizeof(eh_schema))||!add(fields,sizeof(eh_field))||
               !add(count==0u?0u:records,sizeof(eh_record))||!add(payload,1u)||largest>payload)return false;
            eh_schemas_.resize(count);eh_fields_.resize(fields);
            eh_records_.resize(count==0u?0u:records);eh_payload_.resize(payload);
            ::std::size_t field_at{};
            for(::std::size_t i{};i<count;++i)
            {
                auto tag=get(i);auto index=type_index(i);::std::size_t bytes{};if(!shape(*tag,index,bytes))return false;
                auto const& params=tag->function_type_ptr->parameter;
                auto n=params.begin==params.end?0u:static_cast<::std::size_t>(params.end-params.begin);
                eh_schemas_[i]={tag,field_at,n,bytes};
                for(::std::size_t k{};k<n;++k)
                {
                    auto code=static_cast<unsigned>(params.begin[k]);bool reference=code==0x69u;
                    auto width=reference?sizeof(eh_reference):(code==0x7fu||code==0x7du?4u:code==0x7bu?16u:8u);
                    bool nullable=reference&&types.owned_signature_begin[index].parameters.index_unchecked(k).nullable;
                    bool nonnull=reference&&types.owned_signature_begin[index].parameters.index_unchecked(k).heap.code==
                        static_cast<::std::int64_t>(::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type::exn);
                    eh_fields_[field_at++]={static_cast<::std::uint8_t>(width),reference,nullable,nonnull};
                }
            }
            return field_at==fields;
        }
        static ::std::uintptr_t exception_pending() noexcept
        {return active_==nullptr||stopped()?0u:active_->child->eh_pending_;}
        static void exception_publish(::std::uintptr_t module,::std::uintptr_t tag,
            ::std::uintptr_t address,::std::size_t bytes) noexcept
        {
            if(active_==nullptr||stopped())return;auto& child=*active_->child;
            auto schema=child.exception_schema(tag);
            if(module!=0u||schema==nullptr||schema->bytes!=bytes||child.eh_pending_!=0u||
               !child.exception_tuple_valid(*schema,reinterpret_cast<::std::byte const*>(address)))
            {if(!stopped())exception_failure(eh_failure::invalid);return;}
            if(child.eh_record_used_==child.eh_records_.size()||bytes>child.eh_payload_.size()-child.eh_payload_used_)
            {exception_failure(eh_failure::resource);return;}
            auto offset=child.eh_payload_used_;
            if(bytes!=0u)::std::memcpy(child.eh_payload_.data()+offset,reinterpret_cast<void const*>(address),bytes);
            child.eh_records_[child.eh_record_used_]={schema->tag,offset,bytes};
            child.eh_payload_used_+=bytes;child.eh_pending_=++child.eh_record_used_;
        }
        static ::std::uintptr_t exception_matches(::std::uintptr_t token,::std::uintptr_t tag) noexcept
        {
            if(active_==nullptr||stopped())return 0u;auto& child=*active_->child;
            auto record=child.exception_record(token);auto schema=child.exception_schema(tag);
            if(token!=child.eh_pending_||record==nullptr||schema==nullptr)
            {exception_failure(eh_failure::invalid);return 0u;}
            return record->tag->exception_identity.get()==schema->tag->exception_identity.get()?1u:0u;
        }
        static void exception_copy(::std::uintptr_t token,::std::uintptr_t tag,
            ::std::uintptr_t destination,::std::size_t bytes) noexcept
        {
            // Emitter supplies the complete private destination extent. Zero
            // first, so even a stop/invariant edge never creates poison bits.
            if(bytes!=0u&&destination!=0u)::std::memset(reinterpret_cast<void*>(destination),0,bytes);
            if(active_==nullptr||stopped())return;auto& child=*active_->child;
            auto record=child.exception_record(token);auto schema=child.exception_schema(tag);
            if(token!=child.eh_pending_||record==nullptr||schema==nullptr||bytes!=schema->bytes||bytes!=record->bytes||
               (bytes!=0u&&destination==0u)||record->tag->exception_identity.get()!=schema->tag->exception_identity.get()||
               record->offset>child.eh_payload_used_||bytes>child.eh_payload_used_-record->offset)
            {exception_failure(eh_failure::invalid);return;}
            if(bytes!=0u)::std::memcpy(reinterpret_cast<void*>(destination),child.eh_payload_.data()+record->offset,bytes);
        }
        static void exception_make_reference(::std::uintptr_t token,::std::uintptr_t destination) noexcept
        {
            eh_reference ref{};
            if(active_==nullptr||destination==0u)return;
            auto& child=*active_->child;auto record=child.exception_record(token);
            if(!stopped())
            {
                if(token!=child.eh_pending_||record==nullptr)exception_failure(eh_failure::invalid);
                else{ref.storage.ptr=const_cast<eh_record*>(record);ref.kind=eh_kind::wasm_exn;}
            }
            ::std::memcpy(reinterpret_cast<void*>(destination),&ref,sizeof(ref));
        }
        static void exception_consume(::std::uintptr_t token) noexcept
        {
            if(active_==nullptr||stopped())return;auto& child=*active_->child;
            if(token==0u||token!=child.eh_pending_||child.exception_record(token)==nullptr)
            {exception_failure(eh_failure::invalid);return;}
            child.eh_pending_=0u; // immutable arena owners remain for catch_ref.
        }
        static void exception_throw_reference(::std::uintptr_t module,::std::uintptr_t source) noexcept
        {
            if(active_==nullptr||stopped())return;auto& child=*active_->child;
            eh_reference ref{};if(source!=0u)::std::memcpy(&ref,reinterpret_cast<void const*>(source),sizeof(ref));
            if(ref.kind==eh_kind::wasm_null||(ref.kind==eh_kind::wasm_exn&&ref.storage.ptr==nullptr))
            {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::null_reference));return;}
            auto token=child.reference_token(ref);
            if(module!=0u||source==0u||child.eh_pending_!=0u||token==0u)
            {exception_failure(eh_failure::invalid);return;}
            child.eh_pending_=token;
        }

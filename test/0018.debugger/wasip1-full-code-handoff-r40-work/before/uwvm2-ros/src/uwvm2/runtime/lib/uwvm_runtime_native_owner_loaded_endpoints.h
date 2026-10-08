// Cold runtime-private actual object-load observation. No public serialized
// image, table field or nominal function range is an executable permission.
#pragma once
#include "uwvm_runtime_native_owner_function_claims.h"
#ifndef UWVM_MODULE
#include <memory>
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/ExecutionEngine/JITEventListener.h>
# include <llvm/ExecutionEngine/RuntimeDyld.h>
#endif
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
namespace uwvm2::runtime::lib { extern "C++" { class runtime_checkpoint_staged_llvm_full_engine; } }
namespace uwvm2::uwvm::runtime::initializer { extern "C++" { class staged_compiler_module_owner; } }
namespace uwvm2::runtime::lib::details
{
    struct native_owner_publication_bridge; // defined only in real runtime TU
    class pending_actual_native_endpoints final : public ::llvm::JITEventListener
    {
        friend struct native_owner_publication_bridge;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_staged_llvm_full_engine;
        struct expected_function
        {
            ::std::string original_ir_name{};
            ::std::size_t module{}, function{};
            ::std::uint_least64_t generation{};
            unsigned role{};
            ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
        };
        struct loaded_body
        {
            ::std::string original_ir_name{}, entry_object_name{}, local_entry_object_name{};
            ::std::uintptr_t code_begin{}, code_end{}, callable_entry{};
            ::std::uint64_t object_key{}, section{};
            ::std::size_t expected_index{(::std::numeric_limits<::std::size_t>::max)()};
            unsigned role{};
        };
        ::llvm::ExecutionEngine* engine_{};
        void const* code_publication_{}; // comparison only; never dereferenced here
        ::uwvm2::uwvm::runtime::full::full_source_instance::owner source_{};
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module_{};
        ::std::size_t module_id_{(::std::numeric_limits<::std::size_t>::max)()};
        ::std::uint_least64_t runtime_epoch_{};
        ::std::vector<expected_function> expected_{};
        ::std::vector<unsigned> seen_{};
        ::std::vector<loaded_body> bodies_{};
        ::std::vector<::std::uint64_t> objects_{};
        // Retain the complete actual function/unknown/zero symbol ledger DATA.
        // Consumers cannot silently drop aliases by accepting only named rows.
        ::std::vector<native_owner_function_claims::image> claims_{};
        ::std::size_t total_symbols_{},total_object_bytes_{};
        bool valid_{}, attached_{}, sealed_{};
        // Staged observation is distinct from the LIVE publication seal.
        bool staged_only_{},staged_frozen_{};
        ::uwvm2::runtime::checkpoint::compilation_profile::owner staged_profile_{};
        using staged_native_charge=bool (*)(void*,::std::size_t,::std::size_t) noexcept;
        void* staged_budget_owner_{};
        staged_native_charge staged_charge_{};
#if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        pending_actual_native_endpoints(::llvm::ExecutionEngine&,void const*,
            ::uwvm2::uwvm::runtime::initializer::staged_compiler_module_owner const&,
            ::uwvm2::runtime::checkpoint::compilation_profile::owner,::std::size_t,
            ::std::vector<expected_function>,void*,staged_native_charge);
#endif
        inline static constexpr ::std::size_t max_objects{4096u},max_total_object_bytes{256u*1024u*1024u};

        // ONLY real runtime bridge may construct this after capturing the actual
        // compiler's closed declarations before IR handoff/parallel reset. It
        // must compare actual rec/published source/plan/generation under the real
        // runtime publication guard; this private factory accepts no Wasm data.
        pending_actual_native_endpoints(::llvm::ExecutionEngine& actual_engine,void const* actual_code_owner,
            ::uwvm2::uwvm::runtime::full::full_source_instance::owner source,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module,
            ::std::size_t actual_id,::std::uint_least64_t actual_epoch,::std::vector<expected_function> expected)
            : engine_{::std::addressof(actual_engine)},code_publication_{actual_code_owner},source_{::std::move(source)},
              module_{actual_module},module_id_{actual_id},runtime_epoch_{actual_epoch},expected_{::std::move(expected)}
        {
            if(actual_code_owner==nullptr || actual_epoch==0u || actual_id==(::std::numeric_limits<::std::size_t>::max)() ||
               !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source_) ||
               source_->bound_initialized_file(actual_id,actual_module)==nullptr || expected_.empty() ||
               expected_.size()>native_owner_object_graph::detail::max_rows) { return; }
            ::std::sort(expected_.begin(),expected_.end(),[](auto const& a,auto const& b){ return a.original_ir_name<b.original_ir_name; });
            for(::std::size_t i{};i<expected_.size();++i)
            {
                auto const& function{expected_[i]};
                if(function.original_ir_name.empty() || function.original_ir_name.size()>native_owner_table_format::max_name_bytes ||
                   function.module!=actual_id || function.generation==0u || (function.role!=1u && function.role!=2u) ||
                   (i!=0u && expected_[i-1u].original_ir_name==function.original_ir_name)) { return; }
                if(function.plan && (function.plan->get().module!=function.module || function.plan->get().function!=function.function ||
                   function.plan->get().function_generation!=function.generation)) { return; }
                if(function.role==2u && (!function.plan || function.plan->get().resume_abi_revision!=2u ||
                   function.plan->get().resume_sites.empty() || function.plan->get().module!=function.module || function.plan->get().function!=function.function ||
                   function.plan->get().function_generation!=function.generation)) { return; }
            }
            seen_.resize(expected_.size());valid_=true;
            actual_engine.RegisterJITEventListener(this);attached_=true;
        }
        void detach() noexcept
        {
            if(!attached_) { return; }
            // [same actually adopted/live engine] unregister before observer dies.
            engine_->UnregisterJITEventListener(this);attached_=false;
        }
        void revoke() noexcept
        {
            valid_=false;sealed_=false;staged_frozen_=false;bodies_.clear();objects_.clear();claims_.clear();seen_.clear();total_symbols_=0u;total_object_bytes_=0u;
        }
        [[nodiscard]] bool collect_loaded(ObjectKey key,::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded)
        {
            if(!valid_ || sealed_ || staged_frozen_ || !attached_ || engine_==nullptr || key==0u ||
               objects_.size()==max_objects || object.getBytesInAddress()!=sizeof(::std::uintptr_t)) { return false; }
            for(auto const previous:objects_) { if(previous==key) { return false; } }
            if(staged_only_)
            {
                // Reserve bounded object/graph workspace before decoding or
                // copying the complete claim ledger. Charge repeated symbol
                // names individually rather than trusting string-table size.
                if(!staged_budget_owner_ || !staged_charge_ || object.getData().size()>max_total_object_bytes ||
                   !staged_charge_(staged_budget_owner_,object.getData().size(),16u)) { return false; }
                ::std::size_t symbols{};
                for(auto const& symbol:object.symbols())
                {
                    if(symbols==native_owner_function_claims::detail::max_symbols) { return false; }
                    ++symbols;auto name=symbol.getName();
                    if(!name) { ::llvm::consumeError(name.takeError());return false; }
                    if(name->size()>native_owner_table_format::max_name_bytes ||
                       !staged_charge_(staged_budget_owner_,1u,2u*sizeof(native_owner_function_claims::claim)) ||
                       !staged_charge_(staged_budget_owner_,name->size(),4u)) { return false; }
                }
            }
            native_owner_function_claims::image claims{};
            if(!native_owner_function_claims::collect(object,claims) || !claims.endpoints_unambiguous() ||
               bodies_.size()>native_owner_object_graph::detail::max_rows ||
               claims.bodies.size()>native_owner_object_graph::detail::max_rows-bodies_.size() ||
               total_symbols_>native_owner_function_claims::detail::max_symbols ||
               claims.claims.size()>native_owner_function_claims::detail::max_symbols-total_symbols_ ||
               total_object_bytes_>max_total_object_bytes || object.getData().size()>max_total_object_bytes-total_object_bytes_)
            { return false; }
            struct loaded_section { ::std::uint64_t index{},size{},load{};bool text{}; };
            ::std::vector<loaded_section> sections{};
            for(auto const& section:object.sections())
            {
                if(sections.size()==native_owner_object_graph::detail::max_sections) { return false; }
                sections.push_back({section.getIndex(),section.getSize(),loaded.getSectionLoadAddress(section),section.isText()});
            }
            ::std::sort(sections.begin(),sections.end(),[](auto const& a,auto const& b){ return a.index<b.index; });
            for(::std::size_t i{1u};i<sections.size();++i) { if(sections[i-1u].index==sections[i].index) { return false; } }
            auto section_for=[&](::std::uint64_t index) -> loaded_section const*
            {
                auto found{::std::lower_bound(sections.begin(),sections.end(),index,
                    [](auto const& value,auto key_index){ return value.index<key_index; })};
                if(found==sections.end() || found->index!=index) { return nullptr; }
                // [owned complete actual section table][real matching element]
                // [safe] sentinel/index check BEFORE taking its synchronous address.
                return ::std::addressof(*found);
            };
            ::std::vector<::std::size_t> claim_indices{};claim_indices.reserve(claims.claims.size());
            for(::std::size_t i{};i<claims.claims.size();++i) { claim_indices.push_back(i); }
            ::std::sort(claim_indices.begin(),claim_indices.end(),[&](auto a,auto b)
                { return claims.claims[a].name<claims.claims[b].name; });
            constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            for(auto const& row:claims.bodies)
            {
                if(!row.endpoints_proved || row.shape!=1u || row.begin_offset>=row.end_offset) { return false; }
                auto const* actual_section{section_for(row.section)};
                if(actual_section==nullptr || !actual_section->text || actual_section->size!=row.section_size ||
                   row.end_offset>actual_section->size || actual_section->load==0u || actual_section->load>limit ||
                   row.section_size>limit-actual_section->load || row.end_offset>limit-actual_section->load) { return false; }
                auto const actual_load{actual_section->load};
                // [actual loaded TEXT section][checked begin/end offsets]
                // [safe] full section load fits uintptr and offsets within section
                // BEFORE scalar relocation. No pointer/code byte is formed/read.
                auto const begin{actual_load+row.begin_offset},end{actual_load+row.end_offset};
                auto const found{::std::lower_bound(claim_indices.begin(),claim_indices.end(),::std::string_view{row.entry_object_name},
                    [&](auto index,auto name){ return ::std::string_view{claims.claims[index].name}<name; })};
                if(found==claim_indices.end() || *found>=claims.claims.size()) { return false; }
                auto const& claim{claims.claims[*found]};
                if(claim.name!=row.entry_object_name || !claim.defined || !claim.located) { return false; }
                // [same owned index array ... found != end][optional next][end]
                // [safe] found!=end before forming its one-past successor; compare
                // complete name only if that successor remains within the array.
                auto next{found};++next;
                if(next!=claim_indices.end() && claims.claims[*next].name==claim.name) { return false; }
                auto const* entry_section{section_for(claim.section)};
                if(entry_section==nullptr || entry_section->size!=claim.section_size || claim.offset>=claim.section_size ||
                   entry_section->load==0u || entry_section->load>limit || claim.section_size>limit-entry_section->load ||
                   claim.offset>limit-entry_section->load) { return false; }
                // [real callable entry section+offset] separate from true code.
                // [safe] PPC ELFv1 .opd remains DATA; never dereference word0 or
                // invoke native_function_code_address on this already TEXT range.
                auto const callable{static_cast<::std::uintptr_t>(entry_section->load+claim.offset)};
                if(callable==0u || (claim.text && callable!=begin)) { return false; }
                auto const selected{::std::lower_bound(expected_.begin(),expected_.end(),::std::string_view{row.original_ir_name},
                    [](auto const& value,auto key_name){ return ::std::string_view{value.original_ir_name}<key_name; })};
                auto index{(::std::numeric_limits<::std::size_t>::max)()};
                if(row.role==1u || row.role==2u)
                {
                    if(selected==expected_.end() || selected->original_ir_name!=row.original_ir_name || selected->role!=row.role) { return false; }
                    // [owned expected begin ... actual selected != end]
                    // [safe] difference is taken only within this same array after
                    // the sentinel/complete name checks; proves index<seen_.size.
                    index=static_cast<::std::size_t>(selected-expected_.begin());
                    if(index>=seen_.size() || ++seen_[index]!=1u) { return false; }
                }
                else if(selected!=expected_.end() && selected->original_ir_name==row.original_ir_name) { return false; }
                // Raw adapters/other helpers remain overlap claims, never granted
                // guest contexts. All actual bodies are retained through seal.
                bodies_.push_back({row.original_ir_name,row.entry_object_name,row.local_entry_object_name,
                    static_cast<::std::uintptr_t>(begin),static_cast<::std::uintptr_t>(end),callable,key,row.section,index,row.role});
            }
            total_symbols_+=claims.claims.size();total_object_bytes_+=object.getData().size();
            objects_.push_back(key);claims_.push_back(::std::move(claims));return true;
        }
        [[nodiscard]] bool seal_actual_adopted_owner(::llvm::ExecutionEngine const& actual_adopted,
            void const* actual_code_owner,::std::uint_least64_t actual_epoch) noexcept
        {
            if(staged_only_ || !valid_ || sealed_ || !attached_ || ::std::addressof(actual_adopted)!=engine_ ||
               actual_code_owner!=code_publication_ || actual_epoch!=runtime_epoch_ || objects_.empty() ||
               actual_adopted.hasError() ||
               !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source_) ||
               source_->actual_validated_file(module_id_,actual_epoch,module_)==nullptr || expected_.size()!=seen_.size()) { return false; }
            for(auto const count:seen_) { if(count!=1u) { return false; } }
            ::std::sort(bodies_.begin(),bodies_.end(),[](auto const& a,auto const& b){ return a.code_begin<b.code_begin; });
            for(::std::size_t i{};i<bodies_.size();++i)
            {
                auto const& body{bodies_[i]};
                if(body.code_begin==0u || body.code_end<=body.code_begin ||
                   (i!=0u && bodies_[i-1u].code_end>body.code_begin)) { return false; }
            }
            // Actual observer detaches only AFTER complete all-object/endpoints,
            // closed expected declarations, source validation and real adoption.
            // No native CodeSite/trap is minted here; runtime still separately
            // canonicalizes exact stopped capture/engine/gen/epoch and MC successor.
            detach();sealed_=true;return true;
        }
    public:
        pending_actual_native_endpoints(pending_actual_native_endpoints const&)=delete;
        pending_actual_native_endpoints& operator=(pending_actual_native_endpoints const&)=delete;
        ~pending_actual_native_endpoints() { detach(); }
        void notifyObjectLoaded(ObjectKey key,::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        {
#ifdef UWVM_CPP_EXCEPTIONS
            try { if(!collect_loaded(key,object,loaded)) { revoke(); } }
            catch(...) { revoke(); }
#else
            static_cast<void>(key);static_cast<void>(object);static_cast<void>(loaded);revoke();
#endif
        }
        void notifyFreeingObject(ObjectKey) noexcept override { revoke(); }
        // No public factory, numeric-PC lookup or field getter. Only the genuine
        // runtime bridge can take sealed rows into actual canonical publication.
    };
}
#endif

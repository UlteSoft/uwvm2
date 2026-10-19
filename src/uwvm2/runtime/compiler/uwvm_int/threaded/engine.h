#pragma once
#include "program.h"
#include <array>
#include <atomic>
#include <new>
#include <utility>
#include <cmath>
#include <uwvm2/runtime/compiler/shared/strict_float.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <uwvm2/runtime/wasm_threads/wait.h>
#include <uwvm2/object/memory/linear/threaded_grow_types.h>

#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE)
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    namespace threads = ::uwvm2::runtime::wasm_threads;
    class engine;
    struct compile_result { execution_result outcome{}; ::std::unique_ptr<engine> code{}; };
    class engine final
    {
        using memory_type = ::uwvm2::object::memory::linear::native_memory_t;
        using global_type = ::uwvm2::object::global::wasm_global_storage_t;
        struct target { ::std::size_t module{}, function{}; bool host{}; signature const* type{}; };
        struct bound_module
        {
            module_program program{};
            memory_type* memory{};
            ::std::size_t maximum_memory_bytes{};
            ::std::vector<target> targets{};
            ::std::vector<global_type*> globals{};
            ::std::vector<storage::local_defined_tag_storage_t const*> tags{};
        };
        struct activation
        {
            ::std::size_t module{}, function{}, pc{}, locals{}, base{}, previous_values{}, controls{};
        };
        struct control
        {
            ::std::size_t base{}, start{}, end{}, parameters{}, results{};
            bool loop{};
            instruction const* handlers{};
        };
        struct exception_record { storage::local_defined_tag_storage_t const* tag{}; ::std::size_t offset{},count{}; };
        child_world::owner world_;
        decltype(::std::declval<child_world const&>().validated_code()) validated_code_;
        limits limits_;
        ::std::vector<bound_module> modules_{};
        // These fixed-size arrays are allocated before a native child is minted.
        // Guest recursion consumes explicit slots, not recursive C++ frames.
        ::std::vector<::std::uint64_t> values_{};
        ::std::vector<activation> calls_{};
        ::std::vector<control> controls_{};
        ::std::vector<::std::byte> host_parameters_{},host_results_{};
        ::std::vector<exception_record> exceptions_{};
        ::std::vector<::std::uint64_t> exception_values_{};
        ::std::size_t exception_top_{},exception_value_top_{};
        ::std::size_t value_top_{}, call_top_{}, control_top_{};
        ::std::atomic_flag entered_{};
        execution_result result_{};
        ::uwvm2::object::memory::linear::threaded_grow_result last_grow_{};
        // Only the real Group native scheduler may enter this engine. Serial
        // initializer -> command/start is allowed; concurrent/reentrant entry is
        // not. Clear every activation-owned byte, never the retained World.
        void clear_activation() noexcept
        {
            ::std::fill(values_.begin(),values_.end(),0u);
            ::std::fill(calls_.begin(),calls_.end(),activation{});
            ::std::fill(controls_.begin(),controls_.end(),control{});
            ::std::fill(host_parameters_.begin(),host_parameters_.end(),::std::byte{});
            ::std::fill(host_results_.begin(),host_results_.end(),::std::byte{});
            ::std::fill(exceptions_.begin(),exceptions_.end(),exception_record{});
            ::std::fill(exception_values_.begin(),exception_values_.end(),0u);
            value_top_=call_top_=control_top_=exception_top_=exception_value_top_=0;
        }
        struct entry_scope final
        {
            engine& actual;
            ~entry_scope() noexcept { actual.clear_activation();actual.entered_.clear(::std::memory_order_release); }
        };
        explicit engine(child_world::owner world, limits budget):world_{::std::move(world)},limits_{budget} {}

        [[nodiscard]] signature const* function_type(::std::size_t m, ::std::size_t f) const noexcept
        {
            auto const* module{world_->module(m)};if(!module) { return nullptr; }
            auto const n{module->imported_function_vec_storage.size()};
            if(f<n)
            {
                auto const* p{module->imported_function_vec_storage.index_unchecked(f).import_type_ptr};
                return p ? p->imports.storage.function : nullptr;
            }
            f-=n;return f<module->local_defined_function_vec_storage.size() ?
                module->local_defined_function_vec_storage.index_unchecked(f).function_type_ptr : nullptr;
        }
        template<typename T,typename Vector>
        [[nodiscard]] static bool member_index(Vector const& vector, T const* pointer, ::std::size_t& index) noexcept
        {
            auto const n{vector.size()};if(!pointer || !n) { return false; }
            auto const begin{reinterpret_cast<::std::uintptr_t>(vector.data())};
            auto const address{reinterpret_cast<::std::uintptr_t>(pointer)};
            if(address<begin || n>SIZE_MAX/sizeof(T)) { return false; }
            auto const offset{address-begin};
            if(offset>=n*sizeof(T) || offset%sizeof(T)) { return false; }
            index=offset/sizeof(T);return true;
        }
        [[nodiscard]] bool defined_target(storage::local_defined_function_storage_t const* pointer, target& out) const noexcept
        {
            for(::std::size_t m{};m<modules_.size();++m)
            {
                auto const* module{world_->module(m)};::std::size_t f{};
                if(member_index(module->local_defined_function_vec_storage,pointer,f))
                { out={m,module->imported_function_vec_storage.size()+f,false,pointer->function_type_ptr};return true; }
            }
            return false; // In particular, never execute a parent guest pointer.
        }
        [[nodiscard]] bool resolve_target(::std::size_t m, ::std::size_t f, target& out) const noexcept
        {
            auto const* module{world_->module(m)};auto const count{module->imported_function_vec_storage.size()};
            auto const* type{function_type(m,f)};if(!detail::executable_signature(type)) { return false; }
            if(f>=count) { out={m,f,false,type};return true; }
            auto const* record{::std::addressof(module->imported_function_vec_storage.index_unchecked(f))};
            // Every followed record must be an exact element of this child's
            // retained registry. A guessed pointer is not an import identity.
            for(::std::size_t steps{};steps<=limits_.functions;++steps)
            {
                bool owned{};
                for(::std::size_t owner{};owner<modules_.size();++owner)
                { ::std::size_t unused{};if(member_index(world_->module(owner)->imported_function_vec_storage,record,unused)) { owned=true;break; } }
                if(!owned) { return false; }
                using kind=storage::imported_function_link_kind;
                if(record->link_kind==kind::imported) { record=record->target.imported_ptr;continue; }
                if(record->link_kind==kind::defined)
                { return defined_target(record->target.defined_ptr,out) && detail::equal_signature(type,out.type); }
                if(record->link_kind==kind::local_imported)
                { out={m,f,true,type};return record->target.local_imported.module_ptr!=nullptr; }
                return false; // DL/weak/unknown host cancellation is not admitted.
            }
            return false;
        }
        [[nodiscard]] static memory_type* shared_memory(storage::wasm_module_storage_t const& module) noexcept
        {
            if(module.memory_declarations_require_memory64 ||
                module.imported_memory_vec_storage.size()+module.local_defined_memory_vec_storage.size()!=1uz) { return nullptr; }
            if(module.imported_memory_vec_storage.empty())
            {
                auto* memory{const_cast<memory_type*>(::std::addressof(module.local_defined_memory_vec_storage.index_unchecked(0).memory))};
                return threads::is_shared(*memory) && memory->custom_page_size_log2==16u ? memory : nullptr;
            }
            auto const* record{::std::addressof(module.imported_memory_vec_storage.index_unchecked(0))};
            for(::std::size_t i{};i!=4096uz && record;++i)
            {
                using kind=storage::imported_memory_storage_t::imported_memory_link_kind;
                if(record->link_kind==kind::imported) { record=record->target.imported_ptr;continue; }
                if(record->link_kind==kind::defined && record->target.defined_ptr)
                {
                    auto* memory{::std::addressof(record->target.defined_ptr->memory)};
                    return threads::is_shared(*memory) && memory->custom_page_size_log2==16u ? memory : nullptr;
                }
                return nullptr;
            }
            return nullptr;
        }
        [[nodiscard]] static global_type* resolve_global(storage::wasm_module_storage_t const& module, ::std::size_t index) noexcept
        {
            auto const count{module.imported_global_vec_storage.size()};
            if(index>=count)
            {
                index-=count;if(index>=module.local_defined_global_vec_storage.size()) { return nullptr; }
                return const_cast<global_type*>(::std::addressof(module.local_defined_global_vec_storage.index_unchecked(index).global));
            }
            auto const* record{::std::addressof(module.imported_global_vec_storage.index_unchecked(index))};
            for(::std::size_t i{};i!=4096uz && record;++i)
            {
                using kind=storage::imported_global_storage_t::imported_global_link_kind;
                if(record->link_kind==kind::imported) { record=record->target.imported_ptr;continue; }
                if(record->link_kind==kind::defined && record->target.defined_ptr) { return ::std::addressof(record->target.defined_ptr->global); }
                return nullptr;
            }
            return nullptr;
        }
        [[nodiscard]] bool fail(status code) noexcept { result_.code=code;return false; }
        [[nodiscard]] bool push(::std::uint64_t value) noexcept
        { if(value_top_==values_.size()) { return fail(status::resource_limit); } values_[value_top_++]=value;return true; }
        [[nodiscard]] bool pop(::std::uint64_t& out) noexcept
        {
            if(!call_top_ || value_top_<=calls_[call_top_-1uz].base) { return fail(status::invariant_failure); }
            out=values_[--value_top_];return true;
        }
        [[nodiscard]] bool compact(::std::size_t base, ::std::size_t count) noexcept
        {
            if(count>value_top_ || base>value_top_-count || count>values_.size()-base) { return fail(status::invariant_failure); }
            ::std::memmove(values_.data()+base,values_.data()+value_top_-count,count*sizeof(::std::uint64_t));
            value_top_=base+count;return true;
        }
        [[nodiscard]] bool enter(target const& target) noexcept
        {
            if(target.host || target.module>=modules_.size() || call_top_==calls_.size()) { return fail(status::resource_limit); }
            auto const* module{world_->module(target.module)};
            auto const local{target.function-module->imported_function_vec_storage.size()};
            if(local>=modules_[target.module].program.functions.size()) { return fail(status::invariant_failure); }
            auto const& f{modules_[target.module].program.functions[local]};
            auto const arguments{static_cast<::std::size_t>(f.type->parameter.end-f.type->parameter.begin)};
            if(arguments>value_top_ || f.locals<arguments || f.locals>values_.size()-(value_top_-arguments) || control_top_==controls_.size())
            { return fail(status::resource_limit); }
            auto const mark{value_top_-arguments};
            auto const controls{control_top_};
            calls_[call_top_++]={target.module,target.function,0,mark,mark+f.locals,mark,controls};
            ::std::fill(values_.begin()+value_top_,values_.begin()+mark+f.locals,0u);value_top_=mark+f.locals;
            controls_[control_top_++]={value_top_,0,f.instructions.size()-1uz,0,
                static_cast<::std::size_t>(f.type->result.end-f.type->result.begin),false};
            return true;
        }
        [[nodiscard]] bool leave() noexcept
        {
            auto const frame{calls_[call_top_-1uz]};auto const* t{function_type(frame.module,frame.function)};
            if(!compact(frame.previous_values,static_cast<::std::size_t>(t->result.end-t->result.begin))) { return false; }
            control_top_=frame.controls;--call_top_;return true;
        }
        [[nodiscard]] bool branch(::std::size_t depth) noexcept
        {
            auto& frame{calls_[call_top_-1uz]};
            if(depth>=control_top_-frame.controls) { return fail(status::invariant_failure); }
            auto const index{control_top_-1uz-depth};auto const label{controls_[index]};
            if(!compact(label.base,label.loop?label.parameters:label.results)) { return false; }
            if(index==frame.controls) { return leave(); }
            if(label.loop) { control_top_=index+1uz;frame.pc=label.start; }
            else { control_top_=index;frame.pc=label.end+1uz; }
            return true;
        }
        [[nodiscard]] bool host_call(threads::ThreadedGroup::ChildLease& lease, target const& target) noexcept
        {
            auto const* t{target.type};auto const params{static_cast<::std::size_t>(t->parameter.end-t->parameter.begin)};
            auto const results{static_cast<::std::size_t>(t->result.end-t->result.begin)};
            if(params>value_top_-calls_[call_top_-1uz].base || results>values_.size()-(value_top_-params)) { return fail(status::invariant_failure); }
            auto const base{value_top_-params};::std::size_t in{},out{};
            for(::std::size_t i{};i<params;++i)
            {
                auto const bytes{detail::abi_bytes(t->parameter.begin[i])};
                if(bytes>host_parameters_.size()-in) { return fail(status::resource_limit); }
                ::std::memcpy(host_parameters_.data()+in,::std::addressof(values_[base+i]),bytes);in+=bytes;
            }
            for(auto i{t->result.begin};i!=t->result.end;++i) { out+=detail::abi_bytes(*i); }
            if(out>host_results_.size()) { return fail(status::resource_limit); }
            ::std::fill(host_results_.begin(),host_results_.begin()+out,::std::byte{});
            auto const& caller{calls_[call_top_-1uz]};
            ::uwvm2::runtime::lib::details::scoped_llvm_wasm_host_fp_environment_restore host_fp{true};
            if(!host_fp.ready()) { return fail(status::host_failure); }
            auto const result{threads::dispatch_import(lease,*world_,caller.module,caller.function,target.function,
                {host_results_.data(),out},{host_parameters_.data(),in})};
            if(result==threads::host_dispatch_status::group_stopped) { return fail(status::cancelled); }
            if(result!=threads::host_dispatch_status::returned) { return fail(status::host_denied); }
            if(lease.cancellation().stop_requested()) { return fail(status::cancelled); }
            value_top_=base;out=0;
            for(::std::size_t i{};i<results;++i)
            {
                auto const bytes{detail::abi_bytes(t->result.begin[i])};::std::uint64_t value{};
                ::std::memcpy(::std::addressof(value),host_results_.data()+out,bytes);out+=bytes;
                if(!push(value)) { return false; }
            }
            return true;
        }
        template<::std::size_t Bytes>
        [[nodiscard]] bool access(memory_type& memory, ::std::uint64_t offset, bool write, ::std::uint64_t& value) noexcept
        {
            ::std::size_t length{};
            bool const ok{threads::details::access(memory,offset,Bytes,length,[&](::std::byte* pointer) noexcept
            {
                // Guest raw storage uses the same compiler-atomic may_alias
                // convention as shared/wasm_threads.h, never atomic_ref over a
                // fabricated C++ object. Plain Wasm accesses may tear bytewise;
                // the native profile still needs mixed-width race qualification.
                using byte_type=typename ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_access_traits<1>::type;
                for(::std::size_t i{};i!=Bytes;++i)
                {
                    auto* byte{reinterpret_cast<byte_type*>(pointer+i)};
                    if(write) { __atomic_store_n(byte,static_cast<byte_type>(value>>(i*8u)),__ATOMIC_RELAXED); }
                    else { value|=static_cast<::std::uint64_t>(__atomic_load_n(byte,__ATOMIC_RELAXED))<<(i*8u); }
                }
            })};
            return ok || fail(status::trap_memory);
        }
        [[nodiscard]] bool memory_access(instruction const& ins, memory_type& memory) noexcept
        {
            auto const op{ins.opcode};bool const store{op>=0x36u};::std::uint64_t value{},address{};
            if(store && !pop(value)) { return false; } if(!pop(address)) { return false; }
            auto const offset{static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(address))+ins.immediate};
            unsigned width{(op==0x28u||op==0x2au||op==0x36u||op==0x38u||op==0x34u||op==0x35u||op==0x3eu)?4u:
                (op==0x29u||op==0x2bu||op==0x37u||op==0x39u)?8u:(op==0x2cu||op==0x2du||op==0x30u||op==0x31u||op==0x3au||op==0x3cu)?1u:2u};
            bool ok{};switch(width) { case 1:ok=access<1>(memory,offset,store,value);break;case 2:ok=access<2>(memory,offset,store,value);break;
                case 4:ok=access<4>(memory,offset,store,value);break;default:ok=access<8>(memory,offset,store,value); }
            if(!ok||store) { return ok; }
            bool const sign{op==0x2cu||op==0x2eu||op==0x30u||op==0x32u||op==0x34u};
            if(sign && width<8u && (value&(::std::uint64_t{1}<<(width*8u-1u)))) { value|=(~::std::uint64_t{})<<(width*8u); }
            if(op==0x28u||(op>=0x2cu&&op<=0x2fu)) { value=static_cast<::std::uint32_t>(value); }
            return push(value);
        }
        template<::std::size_t Bytes>
        [[nodiscard]] bool atomic_access(memory_type& memory, unsigned operation, ::std::uint64_t offset,
                                         ::std::uint64_t& value, ::std::uint64_t expected) noexcept
        {
            if(offset&(Bytes-1uz)) { return fail(status::trap_unaligned); }
            ::std::size_t length{};namespace atomic=::uwvm2::runtime::compiler::shared::wasm_threads;
            bool const ok{threads::details::access(memory,offset,Bytes,length,[&](::std::byte* p) noexcept
            {
                using op=atomic::atomic_rmw_operation;
                switch(operation)
                {
                    case 0:value=atomic::atomic_load_le<Bytes>(p);break;
                    case 1:atomic::atomic_store_le<Bytes>(p,value);break;
                    case 2:value=atomic::atomic_rmw_le<op::add,Bytes>(p,value);break;
                    case 3:value=atomic::atomic_rmw_le<op::sub,Bytes>(p,value);break;
                    case 4:value=atomic::atomic_rmw_le<op::and_,Bytes>(p,value);break;
                    case 5:value=atomic::atomic_rmw_le<op::or_,Bytes>(p,value);break;
                    case 6:value=atomic::atomic_rmw_le<op::xor_,Bytes>(p,value);break;
                    case 7:value=atomic::atomic_rmw_le<op::exchange,Bytes>(p,value);break;
                    case 8:value=atomic::atomic_rmw_le<op::compare_exchange,Bytes>(p,value,expected);break;
                }
            })};
            return ok || fail(status::trap_memory);
        }
        [[nodiscard]] bool atomic(instruction const& ins, memory_type& memory) noexcept
        {
#ifdef UWVM_CPP_EXCEPTIONS
            try
            {
#endif
            auto const sub{ins.opcode-0x100u};if(sub==3u) { ::std::atomic_thread_fence(::std::memory_order_seq_cst);return true; }
            ::std::uint64_t address{},value{},expected{},timeout{};
            if(sub<=2u)
            {
                if(sub && !pop(timeout)) { return false; }if(!pop(expected)||!pop(address)) { return false; }
                auto const offset{static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(address))+ins.immediate};
                threads::wait_result result{};
                if(sub==0u) { result=threads::memory_notify(memory,offset,static_cast<::std::uint32_t>(expected)); }
                else if(sub==1u) { result=threads::memory_wait<4>(memory,true,offset,expected,::std::bit_cast<::std::int64_t>(timeout)); }
                else { result=threads::memory_wait<8>(memory,true,offset,expected,::std::bit_cast<::std::int64_t>(timeout)); }
                if(result.status==threads::wait_status::cancelled) { return fail(status::cancelled); }
                if(result.status==threads::wait_status::unaligned) { return fail(status::trap_unaligned); }
                if(result.status==threads::wait_status::out_of_bounds) { return fail(status::trap_memory); }
                if(result.status>threads::wait_status::timed_out) { return fail(status::host_failure); }
                return push(sub?static_cast<unsigned>(result.status):result.notified);
            }
            unsigned const slot{(sub-0x10u)%7u};unsigned const operation{(sub-0x10u)/7u};
            if(operation && !pop(value)) { return false; }
            if(operation==8u && !pop(expected)) { return false; }
            if(!pop(address)) { return false; }
            auto const offset{static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(address))+ins.immediate};
            unsigned const widths[7]{4,8,1,2,1,2,4};bool ok{};
            switch(widths[slot])
            { case 1:ok=atomic_access<1>(memory,operation,offset,value,expected);break;
              case 2:ok=atomic_access<2>(memory,operation,offset,value,expected);break;
              case 4:ok=atomic_access<4>(memory,operation,offset,value,expected);break;
              default:ok=atomic_access<8>(memory,operation,offset,value,expected); }
            if(!ok||operation==1u) { return ok; }
            return push(slot==0u||slot==2u||slot==3u?static_cast<::std::uint32_t>(value):value);
#ifdef UWVM_CPP_EXCEPTIONS
            }
            catch(::std::bad_alloc const&) { return fail(status::resource_limit); }
            catch(...) { return fail(status::host_failure); }
#endif
        }
        [[nodiscard]] bool bulk_memory(threads::ThreadedGroup::ChildLease& lease,instruction const& ins) noexcept;
        template<typename Memory>
        [[nodiscard]] bool grow_memory(threads::ThreadedGroup::ChildLease& lease,Memory& memory,::std::size_t maximum) noexcept;
        [[nodiscard]] bool arithmetic(unsigned op) noexcept;
        [[nodiscard]] bool floating(unsigned op) noexcept;
        [[nodiscard]] bool raise(threads::ThreadedGroup::ChildLease&,::std::uint64_t) noexcept;
        [[nodiscard]] bool throw_tag(threads::ThreadedGroup::ChildLease&,::std::size_t) noexcept;
        [[nodiscard]] bool step(threads::ThreadedGroup::ChildLease&, instruction const&) noexcept;
        [[nodiscard]] status prepare(::uwvm2::validation::error::code_validation_error_impl&);
    public:
        engine(engine const&)=delete;engine& operator=(engine const&)=delete;
        [[nodiscard]] static compile_result compile(child_world::owner world, limits budget,
            ::uwvm2::validation::error::code_validation_error_impl& error) noexcept
        {
            compile_result result{};
#ifdef UWVM_CPP_EXCEPTIONS
            try
            {
#if defined(__FAST_MATH__)
                result.outcome.code=status::unsupported_profile;return result;
#endif
                if constexpr(::std::endian::native!=::std::endian::little || sizeof(float)!=4 || sizeof(double)!=8 ||
                    !::std::numeric_limits<float>::is_iec559 || !::std::numeric_limits<double>::is_iec559)
                { result.outcome.code=status::unsupported_profile;return result; }
                if(!world || !budget.values || !budget.controls || !budget.calls || !budget.functions || !budget.host_bytes ||
                    !budget.storage_bytes || !budget.source_bytes || !budget.instruction_bytes || !budget.exception_objects || !budget.exception_values) { result.outcome.code=status::invalid_world;return result; }
                auto owned{::std::unique_ptr<engine>{new engine{::std::move(world),budget}}};
                result.outcome.code=owned->prepare(error);
                if(result.outcome.normal()) { result.code=::std::move(owned); }
            }
            catch(::std::bad_alloc const&) { result.outcome.code=status::resource_limit; }
            catch(...) { result.outcome.code=status::validation_failed; }
#else
            // No unavailable exception/unwind build can publish this profile.
            static_cast<void>(world);static_cast<void>(budget);static_cast<void>(error);
            result.outcome.code=status::unsupported_profile;
#endif
            return result;
        }
        [[nodiscard]] child_world const& world() const noexcept { return *world_; }
        [[nodiscard]] child_world::owner retained_world() const noexcept { return world_; }
        // Capacities, not merely live element counts, are charged. These are
        // backend-owned allocations only; the Root adapter must also charge the
        // actual World and native stack against its genuine reservation.
        [[nodiscard]] ::std::size_t owned_storage_bytes() const noexcept
        {
            ::std::size_t total{sizeof(engine)};
            auto add{[&](::std::size_t n,::std::size_t width) noexcept
            { if(n>(SIZE_MAX-total)/width) { total=SIZE_MAX; } else { total+=n*width; } }};
            add(values_.capacity(),sizeof(::std::uint64_t));add(calls_.capacity(),sizeof(activation));add(controls_.capacity(),sizeof(control));
            add(host_parameters_.capacity(),1);add(host_results_.capacity(),1);add(modules_.capacity(),sizeof(bound_module));
            add(exceptions_.capacity(),sizeof(exception_record));add(exception_values_.capacity(),sizeof(::std::uint64_t));
            for(auto const& m:modules_) { add(m.globals.capacity(),sizeof(global_type*));add(m.tags.capacity(),sizeof(storage::local_defined_tag_storage_t*)); }
            return total;
        }
        [[nodiscard]] ::std::size_t owned_code_bytes() const noexcept
        {
            ::std::size_t total{};auto add{[&](::std::size_t n,::std::size_t width) noexcept
            { if(n>(SIZE_MAX-total)/width) { total=SIZE_MAX; } else { total+=n*width; } }};
            for(auto const& m:modules_)
            {
                add(m.targets.capacity(),sizeof(target));add(m.program.functions.capacity(),sizeof(function_program));
                for(auto const& f:m.program.functions)
                { add(f.instructions.capacity(),sizeof(instruction));for(auto const& i:f.instructions) { add(i.labels.capacity(),sizeof(::std::uint32_t));add(i.catches.capacity(),sizeof(catch_clause)); } }
            }
            return total;
        }
        [[nodiscard]] auto const& last_memory_grow() const noexcept { return last_grow_; }
        [[nodiscard]] execution_result run_entry(threads::ThreadedGroup::ChildLease& lease,
            ::std::size_t function, ::std::span<::std::byte> results, ::std::span<::std::byte const> params) noexcept
        {
            auto const module{world_->entry_module_index()};
            if(::std::addressof(lease.world())!=world_.get() || !lease.owns_current_native_thread() ||
                ::std::addressof(lease.admission())!=::std::addressof(world_->admission()) ||
                module>=modules_.size() || function>=modules_[module].targets.size() || modules_[module].targets[function].host)
            { return {status::invalid_world}; }
            if(!validated_code_ || !validated_code_->covers(*world_)) { return {status::invalid_world}; }
            auto const* type{function_type(module,function)};::std::size_t input_bytes{},output_bytes{};
            for(auto i{type->parameter.begin};i!=type->parameter.end;++i) { if(!detail::numeric_type(*i)) { return {status::trap_signature}; } input_bytes+=detail::abi_bytes(*i); }
            for(auto i{type->result.begin};i!=type->result.end;++i) { if(!detail::numeric_type(*i)) { return {status::trap_signature}; } output_bytes+=detail::abi_bytes(*i); }
            if(params.size()!=input_bytes || results.size()!=output_bytes) { return {status::trap_signature}; }
            if(entered_.test_and_set(::std::memory_order_acq_rel)) { return {status::already_entered}; }
            entry_scope retire{*this};
            clear_activation();result_={};last_grow_={};
            if(lease.cancellation().stop_requested()) { result_.code=status::cancelled;return result_; }
            ::std::size_t offset{};
            for(auto i{type->parameter.begin};i!=type->parameter.end;++i)
            {
                auto const bytes{detail::abi_bytes(*i)};::std::uint64_t value{};
                ::std::memcpy(::std::addressof(value),params.data()+offset,bytes);offset+=bytes;values_[value_top_++]=value;
            }
            bool fp_active{};
            ::uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment fp_scope{fp_active};
            if(!fp_scope.ready())
            { result_.code=status::host_failure;lease.request_trap(static_cast<::std::uint32_t>(result_.code),module,function,0);return result_; }
            threads::execution_scope wait_scope{lease.waits(),lease.cancellation()};
            if(enter(modules_[module].targets[function]))
            {
                while(call_top_ && result_.normal())
                {
                    auto& frame{calls_[call_top_-1uz]};auto const* instance{world_->module(frame.module)};
                    auto const& code{modules_[frame.module].program.functions[frame.function-instance->imported_function_vec_storage.size()].instructions};
                    result_.module=frame.module;result_.function=frame.function;
                    if(frame.pc>=code.size()) { static_cast<void>(fail(status::invariant_failure));break; }
                    auto const& ins{code[frame.pc++]};result_.wasm_offset=ins.offset;
                    if(lease.cancellation().stop_requested()) { static_cast<void>(fail(status::cancelled));break; }
                    if(!step(lease,ins)) { break; }
                }
            }
            if(result_.code!=status::ok && result_.code!=status::cancelled)
            { lease.request_trap(static_cast<::std::uint32_t>(result_.code),result_.module,result_.function,result_.wasm_offset); }
            auto const outcome{lease.read_outcome()};
            if(outcome.kind==threads::termination_kind::proc_exit) { result_.code=status::proc_exit;result_.exit_code=outcome.code; }
            else if(outcome.kind!=threads::termination_kind::running && result_.normal()) { result_.code=status::cancelled; }
            if(result_.normal())
            {
                offset=0;::std::size_t index{};
                for(auto i{type->result.begin};i!=type->result.end;++i,++index)
                {
                    auto const bytes{detail::abi_bytes(*i)};
                    ::std::memcpy(results.data()+offset,::std::addressof(values_[index]),bytes);offset+=bytes;
                }
            }
            // Both the actual parent entry and child start retire the SAME safe
            // iterative path; no ordinary parent opfunc is a fallback.
            return result_; // retire clears all per-entry arenas before busy=false.
        }
        [[nodiscard]] execution_result run_thread_start(threads::ThreadedGroup::ChildLease& lease,
            ::std::uint32_t tid, ::std::uint32_t argument) noexcept
        {
            if(tid==0u || tid>=(1u<<29u)) { return {status::invalid_world}; }
            ::std::uint32_t const values[2]{tid,argument};
            return run_entry(lease,world_->thread_start_function_index(),{},
                {reinterpret_cast<::std::byte const*>(values),sizeof(values)});
        }
    };
}

#include "prepare.h"
#include "execute.h"
#include "floating.h"
#include "exception.h"
#include "bulk_memory.h"

#endif // Linux native-thread, C++ exceptions, non-module threaded source profile

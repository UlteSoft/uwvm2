#pragma once
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    inline bool engine::arithmetic(unsigned op) noexcept
    {
        ::std::uint64_t a{},b{},result{};
        bool const wide{(op>=0x50u&&op<=0x5au)||(op>=0x79u&&op<=0x8au)};
        if(op==0x45u || op==0x50u)
        { if(!pop(a)) { return false; }return push(wide?a==0u:static_cast<::std::uint32_t>(a)==0u); }
        if((op>=0x67u&&op<=0x69u)||(op>=0x79u&&op<=0x7bu))
        {
            if(!pop(a)) { return false; }unsigned const which{op-(wide?0x79u:0x67u)};
            if(wide) { result=which==0u ? ::std::countl_zero(a) : which==1u ? ::std::countr_zero(a) : ::std::popcount(a); }
            else { auto const n{static_cast<::std::uint32_t>(a)};result=which==0u ? ::std::countl_zero(n) : which==1u ? ::std::countr_zero(n) : ::std::popcount(n); }
            return push(result);
        }
        if(!pop(b)||!pop(a)) { return false; }
        if((op>=0x46u&&op<=0x4fu)||(op>=0x51u&&op<=0x5au))
        {
            unsigned const which{op-(wide?0x51u:0x46u)};
            auto compare{[&]<typename U>(U x,U y) noexcept
            {
                using S=::std::make_signed_t<U>;auto const sx{::std::bit_cast<S>(x)},sy{::std::bit_cast<S>(y)};
                switch(which) { case 0:return x==y;case 1:return x!=y;case 2:return sx<sy;case 3:return x<y;
                    case 4:return sx>sy;case 5:return x>y;case 6:return sx<=sy;case 7:return x<=y;case 8:return sx>=sy;default:return x>=y; }
            }};
            return push(wide?compare(a,b):compare(static_cast<::std::uint32_t>(a),static_cast<::std::uint32_t>(b)));
        }
        unsigned const which{op-(wide?0x7cu:0x6au)};
        auto compute{[&]<typename U>(U x,U y) noexcept -> bool
        {
            using S=::std::make_signed_t<U>;constexpr unsigned bits{sizeof(U)*8u};
            auto const sx{::std::bit_cast<S>(x)},sy{::std::bit_cast<S>(y)};unsigned const shift{static_cast<unsigned>(y&(bits-1u))};U z{};
            switch(which)
            {
                case 0:z=static_cast<U>(x+y);break;case 1:z=static_cast<U>(x-y);break;case 2:z=static_cast<U>(x*y);break;
                case 3:case 4:case 5:case 6:
                    if(!y) { return fail(status::trap_divide_by_zero); }
                    if((which==3u||which==5u)&&sx==::std::numeric_limits<S>::min()&&sy==S{-1})
                    { if(which==3u) { return fail(status::trap_integer_overflow); }z=0;break; }
                    if(which==3u) { z=::std::bit_cast<U>(static_cast<S>(sx/sy)); }
                    if(which==4u) { z=x/y; }if(which==5u) { z=::std::bit_cast<U>(static_cast<S>(sx%sy)); }if(which==6u) { z=x%y; }break;
                case 7:z=x&y;break;case 8:z=x|y;break;case 9:z=x^y;break;
                case 10:z=static_cast<U>(x<<shift);break;case 11:z=::std::bit_cast<U>(static_cast<S>(sx>>shift));break;
                case 12:z=x>>shift;break;case 13:z=::std::rotl(x,static_cast<int>(shift));break;case 14:z=::std::rotr(x,static_cast<int>(shift));break;
                default:return fail(status::invariant_failure);
            }
            result=z;return true;
        }};
        if(!(wide?compute(a,b):compute(static_cast<::std::uint32_t>(a),static_cast<::std::uint32_t>(b)))) { return false; }
        return push(result);
    }

    inline bool engine::step(threads::ThreadedGroup::ChildLease& lease, instruction const& ins) noexcept
    {
        auto& frame{calls_[call_top_-1uz]};auto& bound{modules_[frame.module]};auto const* instance{bound.program.instance};
        auto const op{ins.opcode};::std::uint64_t a{},b{},c{};
        if(op>=0x300u) { return bulk_memory(lease,ins); }
        if(op>=0x200u) { return floating(op); }
        if(op>=0x100u) { return atomic(ins,*bound.memory); }
        if(op>=0x28u&&op<=0x3eu) { return memory_access(ins,*bound.memory); }
        if((op>=0x45u&&op<=0x5au)||(op>=0x67u&&op<=0x8au)) { return arithmetic(op); }
        if((op>=0x5bu&&op<=0x66u)||(op>=0x8bu&&op<=0xa6u)||(op>=0xa8u&&op<=0xabu)||(op>=0xaeu&&op<=0xbfu)) { return floating(op); }
        switch(op)
        {
            case 0x00:return fail(status::trap_unreachable);
            case 0x01:return true;
            case 0x02:case 0x03:case 0x04:case 0x1f:
            {
                if(op==0x04u&&!pop(a)) { return false; }
                if(control_top_==controls_.size() || ins.parameters>value_top_-frame.base) { return fail(status::resource_limit); }
                controls_[control_top_++]={value_top_-ins.parameters,frame.pc,ins.target,ins.parameters,ins.results,op==0x03u,op==0x1fu?::std::addressof(ins):nullptr};
                if(op==0x04u && !static_cast<::std::uint32_t>(a)) { frame.pc=ins.alternate; }
                return true;
            }
            case 0x05:frame.pc=ins.target;return true;
            case 0x08:return throw_tag(lease,static_cast<::std::size_t>(ins.immediate));
            case 0x0a:if(!pop(a)) { return false; }return raise(lease,a);
            case 0x0b:
            {
                if(control_top_<=frame.controls) { return fail(status::invariant_failure); }
                if(control_top_==frame.controls+1uz) { return leave(); }
                auto const label{controls_[--control_top_]};return compact(label.base,label.results);
            }
            case 0x0c:return branch(static_cast<::std::size_t>(ins.immediate));
            case 0x0d:if(!pop(a)) { return false; }return !static_cast<::std::uint32_t>(a)||branch(static_cast<::std::size_t>(ins.immediate));
            case 0x0e:
                if(!pop(a)||ins.labels.empty()) { return fail(status::invariant_failure); }
                return branch(ins.labels[::std::min(static_cast<::std::size_t>(static_cast<::std::uint32_t>(a)),ins.labels.size()-1uz)]);
            case 0x0f:return leave();
            case 0x10:
            {
                if(ins.immediate>=bound.targets.size()) { return fail(status::invariant_failure); }
                auto const& target{bound.targets[ins.immediate]};return target.host?host_call(lease,target):enter(target);
            }
            case 0x11:
            {
                if(!pop(a)) { return false; }
                if(ins.target>=instance->local_defined_table_vec_storage.size() || ins.immediate>=instance->type_section_storage.type_section_count)
                { return fail(status::trap_table); }
                auto const& table{instance->local_defined_table_vec_storage.index_unchecked(ins.target)};
                auto const index{static_cast<::std::size_t>(static_cast<::std::uint32_t>(a))};
                if(index>=table.elems.size()) { return fail(status::trap_table); }
                auto const& slot{table.elems.index_unchecked(index)};
                if(slot.type==storage::local_defined_table_elem_storage_type_t::func_ref_imported && !slot.storage.imported_ptr)
                { return fail(status::trap_null); }
                if(slot.type!=storage::local_defined_table_elem_storage_type_t::func_ref_defined) { return fail(status::trap_signature); }
                if(!slot.storage.defined_ptr) { return fail(status::trap_null); }
                target target{};if(!defined_target(slot.storage.defined_ptr,target)) { return fail(status::trap_signature); }
                auto const* expected{instance->type_section_storage.type_section_begin+ins.immediate};
                if(!detail::equal_signature(expected,target.type)) { return fail(status::trap_signature); }
                return enter(target);
            }
            case 0x1a:return pop(a);
            case 0x1b:case 0x1c:if(!pop(c)||!pop(b)||!pop(a)) { return false; }return push(static_cast<::std::uint32_t>(c)?a:b);
            case 0x20:case 0x21:case 0x22:
            {
                if(ins.immediate>=frame.base-frame.locals) { return fail(status::invariant_failure); }
                auto& local{values_[frame.locals+ins.immediate]};
                if(op==0x20u) { return push(local); }if(!pop(a)) { return false; }local=a;return op==0x21u||push(a);
            }
            case 0x23:case 0x24:
            {
                if(ins.immediate>=bound.globals.size()) { return fail(status::invariant_failure); }
                auto* global{bound.globals[ins.immediate]};
                using k=::uwvm2::object::global::global_type;
                bool const wide{global->kind==k::wasm_i64 || global->kind==k::wasm_f64};
                void* cell{};
                switch(global->kind)
                {
                    case k::wasm_i32:cell=::std::addressof(global->storage.i32);break;
                    case k::wasm_i64:cell=::std::addressof(global->storage.i64);break;
                    case k::wasm_f32:cell=::std::addressof(global->storage.f32);break;
                    case k::wasm_f64:cell=::std::addressof(global->storage.f64);break;
                    default:return fail(status::invariant_failure);
                }
                if(op==0x23u) { ::std::memcpy(::std::addressof(a),cell,wide?8uz:4uz);return push(a); }
                if(!global->is_mutable || !pop(a)) { return fail(status::invariant_failure); }
                ::std::memcpy(cell,::std::addressof(a),wide?8uz:4uz);return true;
            }
            case 0x3f:
            {
                ::std::size_t length{};if(!threads::details::access(*bound.memory,0,0,length,[](::std::byte*) noexcept{})) { return fail(status::trap_memory); }
                return push(static_cast<::std::uint32_t>(length>>16u));
            }
            case 0x40:return grow_memory(lease,*bound.memory,bound.maximum_memory_bytes);
            case 0x41:return push(static_cast<::std::uint32_t>(ins.immediate));
            case 0x42:case 0x44:return push(ins.immediate);
            case 0x43:return push(static_cast<::std::uint32_t>(ins.immediate));
            case 0xd0:return push(0u);
            case 0xd1:if(!pop(a)) { return false; }return push(a==0u);
            case 0xd4:if(!pop(a)) { return false; }return a?push(a):fail(status::trap_null);
            case 0xd5:if(!pop(a)) { return false; }return a?push(a):branch(static_cast<::std::size_t>(ins.immediate));
            case 0xd6:if(!pop(a)) { return false; }return a?(push(a)&&branch(static_cast<::std::size_t>(ins.immediate))):true;
            case 0xa7:if(!pop(a)) { return false; }return push(static_cast<::std::uint32_t>(a));
            case 0xac:if(!pop(a)) { return false; }return push(static_cast<::std::uint64_t>(static_cast<::std::int64_t>(::std::bit_cast<::std::int32_t>(static_cast<::std::uint32_t>(a)))));
            case 0xad:if(!pop(a)) { return false; }return push(static_cast<::std::uint32_t>(a));
            case 0xc0:case 0xc1:case 0xc2:case 0xc3:case 0xc4:
            {
                if(!pop(a)) { return false; }unsigned const bits{op==0xc0u||op==0xc2u?8u:op==0xc1u||op==0xc3u?16u:32u};
                a&=(::std::uint64_t{1}<<bits)-1u;if(a&(::std::uint64_t{1}<<(bits-1u))) { a|=(~::std::uint64_t{})<<bits; }
                return push(op<=0xc1u?static_cast<::std::uint32_t>(a):a);
            }
            default:return fail(status::invariant_failure);
        }
    }
}

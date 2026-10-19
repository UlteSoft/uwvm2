#pragma once
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    inline bool engine::throw_tag(threads::ThreadedGroup::ChildLease& lease,::std::size_t index) noexcept
    {
        auto const& frame{calls_[call_top_-1uz]};auto const& tags{modules_[frame.module].tags};
        if(index>=tags.size()) { return fail(status::invariant_failure); }
        auto const* tag{tags[index]};auto const* type{tag->function_type_ptr};
        auto const count{static_cast<::std::size_t>(type->parameter.end-type->parameter.begin)};
        if(count>value_top_-frame.base) { return fail(status::invariant_failure); }
        if(exception_top_==exceptions_.size() || count>exception_values_.size()-exception_value_top_)
        { return fail(status::resource_limit); }
        auto const base{value_top_-count};
        exceptions_[exception_top_]={tag,exception_value_top_,count};
        ::std::copy_n(values_.data()+base,count,exception_values_.data()+exception_value_top_);
        exception_value_top_+=count;value_top_=base;
        return raise(lease,++exception_top_); // 0 is only ref.null exn/noexn.
    }

    inline bool engine::raise(threads::ThreadedGroup::ChildLease& lease,::std::uint64_t token) noexcept
    {
        if(token==0u) { return fail(status::trap_null); }
        if(token>exception_top_) { return fail(status::invariant_failure); }
        auto const exception{exceptions_[static_cast<::std::size_t>(token-1u)]};
        // Each exception is immutable in this engine's bounded arena. All its
        // payloads/tokens remain live until entry retirement; no C++ throw or
        // native/parent exception object crosses this explicit unwind.
        auto owner{call_top_};
        for(auto n{control_top_};n!=0uz;)
        {
            --n;if(lease.cancellation().stop_requested()) { return fail(status::cancelled); }
            while(owner!=0uz && n<calls_[owner-1uz].controls) { --owner; }
            if(owner==0uz) { return fail(status::invariant_failure); }
            auto const& label{controls_[n]};if(!label.handlers) { continue; }
            auto const& activation{calls_[owner-1uz]};auto const& tags{modules_[activation.module].tags};
            for(auto const& clause:label.handlers->catches)
            {
                if(lease.cancellation().stop_requested()) { return fail(status::cancelled); }
                bool const tagged{clause.kind<2u},with_reference{(clause.kind&1u)!=0u};
                if(tagged)
                {
                    if(clause.tag>=tags.size()) { return fail(status::invariant_failure); }
                    // Authentic linked INSTANCE identity, not signature/index.
                    if(tags[clause.tag]->exception_identity.get()!=exception.tag->exception_identity.get()) { continue; }
                }
                // Catch depths name OUTER labels, excluding this try_table.
                if(n<=activation.controls || clause.label>=n-activation.controls)
                { return fail(status::invariant_failure); }
                auto const destination{n-1uz-clause.label};
                call_top_=owner;control_top_=destination+1uz;
                value_top_=controls_[destination].base;
                if(tagged)
                {
                    for(::std::size_t i{};i<exception.count;++i)
                    { if(!push(exception_values_[exception.offset+i])) { return false; } }
                }
                if(with_reference && !push(token)) { return false; }
                // Existing tuple branch preserves loop/function/result arity,
                // and removes the try's handlers before any rethrow.
                return branch(0uz);
            }
        }
        return fail(status::trap_uncaught_exception);
    }
}

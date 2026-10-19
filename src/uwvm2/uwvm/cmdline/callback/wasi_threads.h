#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <fast_io.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime_threaded_profile_api.h>
# if UWVM2_HAS_THREADED_PROFILE_HOST_API
#  include <pthread.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM2_HAS_THREADED_PROFILE_HOST_API) && UWVM2_HAS_THREADED_PROFILE_HOST_API
    [[nodiscard]] inline constexpr bool wasi_threads_parse_positive(
        ::uwvm2::utils::container::u8string_view text,::std::size_t& value) noexcept
    {
        if(text.empty())return false;
        ::std::size_t parsed{};
        constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
        for(auto c:text)
        {
            if(c<u8'0'||c>u8'9')return false;
            auto const digit{static_cast<unsigned>(c-u8'0')};
            if(parsed>(maximum-digit)/10u)return false;
            parsed=parsed*10u+digit;
        }
        if(parsed==0u||parsed==maximum)return false;
        value=parsed;return true;
    }
    [[nodiscard]] inline bool wasi_threads_derive_limits(::std::array<::std::size_t,6> const& numbers,
        ::uwvm2::runtime::wasm_threads::threaded_profile_limits& output) noexcept
    {
        using limits=::uwvm2::runtime::wasm_threads::threaded_profile_limits;
        constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
        for(auto n:numbers)if(n==0u||n==maximum)return false;
        auto const active=numbers[0],spawns=numbers[1],stack=numbers[2],storage=numbers[3],code=numbers[4],shared=numbers[5];
        if(active>=(1u<<29u)||spawns>=(1u<<29u)||stack<65536u||stack<static_cast<::std::size_t>(PTHREAD_STACK_MIN)||
           stack%65536u!=0u||shared%65536u!=0u||static_cast<::std::uint64_t>(shared)>(1ull<<32u))return false;
        auto const slots=active+1u; // actual separately owned initial-main0 slot
        auto multiply=[&](::std::size_t each,::std::size_t& product) noexcept
        {if(slots>maximum/each)return false;product=slots*each;return product!=maximum;};
        limits value{};
        value.max_active_children=active;value.max_spawn_attempts=static_cast<::std::uint32_t>(spawns);
        value.native_stack_bytes=stack;value.compiler_stack_bytes=stack;
        value.max_child_storage_bytes=storage;value.max_child_code_bytes=code;
        // N children + initial-main0 + ONE genuine compiler worker. Each
        // value is TOTAL controlled mmap bytes, including its two guard pages.
        // The actual Group/worker also check native page size and usable stack.
        auto const native_slots=slots+1u;
        if(native_slots>maximum/stack)return false;
        value.max_live_native_stack_bytes=native_slots*stack;
        if(value.max_live_native_stack_bytes==maximum||!multiply(storage,value.max_live_storage_bytes)||
           !multiply(code,value.max_live_code_bytes))return false;
        value.max_value_stack_bytes=(::std::min)(storage/16u,static_cast<::std::size_t>(1048576u))&~static_cast<::std::size_t>(7u);
        value.max_control_frames=(::std::min)(storage/4096u,static_cast<::std::size_t>(4096u));
        value.max_call_frames=(::std::min)(storage/65536u,static_cast<::std::size_t>(256u));
        value.max_table_elements=(::std::min)(storage/128u,static_cast<::std::size_t>(65536u));
        value.max_local_memory_bytes=0u;value.max_shared_memory_bytes=shared;
        value.max_total_segment_bytes=(::std::min)(storage/8u,static_cast<::std::size_t>(64u*1024u*1024u));
        auto& init=value.initializer;
        init.max_runtime_modules=2u;
        init.max_imported_functions=(::std::min)(code/4u,static_cast<::std::size_t>(4096u));
        init.max_imported_tables=0u;init.max_imported_memories=1u;init.max_imported_globals=0u;init.max_imported_tags=0u;
        init.max_local_defined_functions=(::std::min)(code/32u,static_cast<::std::size_t>(65536u));
        init.max_local_defined_codes=init.max_local_defined_functions;
        init.max_local_defined_tables=1u;init.max_local_defined_memories=0u;
        init.max_local_defined_globals=(::std::min)(storage/256u,static_cast<::std::size_t>(65536u));
        init.max_local_defined_tags=(::std::min)(storage/256u,static_cast<::std::size_t>(4096u));
        init.max_local_defined_elements=(::std::min)(storage/128u,static_cast<::std::size_t>(65536u));
        init.max_local_defined_datas=(::std::min)(storage/4096u,static_cast<::std::size_t>(64u));
        if(value.max_value_stack_bytes<16u||value.max_control_frames==0u||value.max_call_frames==0u||
           value.max_table_elements==0u||value.max_total_segment_bytes==0u||init.max_imported_functions==0u||
           init.max_local_defined_functions==0u||init.max_local_defined_globals==0u||init.max_local_defined_tags==0u||
           init.max_local_defined_elements==0u||init.max_local_defined_datas==0u)return false;
        // No output mutation until all syntax/derivation checks succeeded.
        // configure_host_api repeats the Group's real actual limits admission.
        output=value;return true;
    }
    inline void wasi_threads_pretreatment(char8_t const* const*& current,char8_t const* const* end,
        ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>& parsed) noexcept
    {
        auto next=current+1u;
        for(unsigned count{};count<6u&&next!=end&&*next!=nullptr;++count,++next)
        {
            auto const text=::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*next)};
            // A following option stays an option; signed numeric input remains
            // an occupied argument for this callback to reject precisely.
            if(!text.empty()&&text.front_unchecked()==u8'-'&&
               (text.size()==1u||text.index_unchecked(1u)<u8'0'||text.index_unchecked(1u)>u8'9'))break;
            // Existing outer parser reserves argc slots before preprocessing;
            // append at most one record per consumed original argv token.
            parsed.emplace_back_unchecked(text,nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
        }
        current=next;
    }
    UWVM_GNU_COLD inline ::uwvm2::utils::cmdline::parameter_return_type wasi_threads_callback(
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* begin,
        ::uwvm2::utils::cmdline::parameter_parsing_results* current,
        ::uwvm2::utils::cmdline::parameter_parsing_results* end) noexcept
    {
        using kind=::uwvm2::utils::cmdline::parameter_parsing_results_type;
        auto usage=[]() noexcept
        {::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,u8"uwvm: [error] --wasi-threads requires six positive decimal finite quotas. Usage: ",
            ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasi_threads),u8"\n\n");};
        ::std::array<::std::size_t,6> numbers{};auto next=current+1u;
        for(auto& number:numbers)
        {
            if(next==end||(next->type!=kind::arg&&next->type!=kind::occupied_arg)||
               !wasi_threads_parse_positive(next->str,number))
            {usage();return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;}
            ++next;
        }
        ::uwvm2::runtime::wasm_threads::threaded_profile_limits limits{};
        if(!wasi_threads_derive_limits(numbers,limits))
        {usage();return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;}
        // Exactly one actual cold Runtime policy publication. A parsed value,
        // caller bool or explicit CLI token alone grants no source/child lease.
        if(!::uwvm2::runtime::lib::runtime_threaded_profile_configure_host_api(limits))
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                u8"uwvm: [error] Cannot configure finite WASI Threads quotas for the actual quiescent Runtime.\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
        for(auto p=current+1u;p!=next;++p)p->type=kind::occupied_arg;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

// Detached policy/cache DATA test only. No checkpoint/restore permission.
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <fast_io.h>
#ifndef UWVM_MODULE
# include <uwvm2/parser/wasm/standard/wasm1p1/features/def.h>
# include <uwvm2/runtime/checkpoint/materialization.h>
# include <uwvm2/runtime/llvm_jit_cache/store.h>
#else
import uwvm2.parser.wasm.standard.wasm1p1.features;
import uwvm2.runtime.checkpoint.materialization;
import uwvm2.runtime.llvm_jit_cache;
#endif
namespace cp = ::uwvm2::runtime::checkpoint;
namespace features = ::uwvm2::parser::wasm::standard::wasm1p1::features;
namespace cache = ::uwvm2::runtime::llvm_jit_cache;
namespace containers = ::uwvm2::utils::container;
using parameters = features::wasm_binfmt1p1_feature_parameter;
static void require(bool okay, char const* message)
{
    if(!okay)
    {
        ::fast_io::io::perrln("module feature policy: ",::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
template<typename A,typename B>
static bool same(A const& a,B const& b)
{ return a.size()==b.size() && ::std::equal(a.cbegin(),a.cend(),b.cbegin()); }
static containers::u8string text(cp::module_feature_policy const& policy)
{
    containers::u8string result{};
    containers::u8string_ref_uwvm output{::std::addressof(result)};
    policy.print_cache_identity_to(output);
    return result;
}
static cache::cache_context context(cp::module_feature_policy const& policy)
{
    cache::cache_context result{};
    result.cache_key=u8"same-original-module-and-generated-ir";
    result.target_triple=u8"component-target";
    result.cpu_name=u8"component-cpu";
    result.cpu_features=u8"component-features";
    result.llvm_version=u8"component-llvm";
    result.uwvm_abi=u8"component-product-native-abi2";
    result.codegen_policy=cache::details::make_cache_key(u8"llvm-jit-full-codegen-policy");
    cache::details::append_cache_key_value(result.codegen_policy,u8"original-module-syntax-policy-v1",text(policy));
    result.cache_key_is_complete=true;
    return result;
}
int main()
{
    constexpr ::std::array<bool parameters::*,19u> members{
        &parameters::disable_multi_value,&parameters::disable_reference_types,
        &parameters::disable_table_instructions,&parameters::disable_multiple_tables,
        &parameters::disable_bulk_memory,&parameters::disable_sign_extension,
        &parameters::disable_nontrapping_float_to_int,&parameters::disable_simd,
        &parameters::disable_extended_const,&parameters::disable_table_initializer,
        &parameters::disable_relaxed_simd,&parameters::disable_multi_memory,
        &parameters::disable_tail_call,&parameters::disable_memory64,&parameters::disable_table64,
        &parameters::disable_function_references,&parameters::disable_gc,
        &parameters::disable_exceptions,&parameters::disable_threads};
    parameters parameter{};
    for(auto member:members) { parameter.*member=false; }
    auto const baseline{cp::module_feature_policy::from_actual_parameter(parameter)};
    require(baseline.valid() && baseline.cache_identity()==cp::module_feature_policy::identity{1u,0u,0u,0u},"actual default fields copied without padding/CLI bookkeeping");
    require(baseline.compatibility_mask()==((::std::uint64_t{1u}<<18u)-1u),"all explicitly enabled capabilities map to known file bits");
    containers::u8string const zero{u8"1/0/0/0"}; require(same(text(baseline),zero),"canonical fast_io decimal policy serializer");
    auto const original{context(baseline)};
    auto const original_metadata{cache::make_context_metadata(original)};
    auto const original_digest{cache::details::cache_key_hash(original)};
    auto distinct=[&](cp::module_feature_policy const& changed)
    {
        require(changed.valid() && changed!=baseline,"field copied to exact valid DATA tuple");
        auto const next{context(changed)};
        require(!same(original_metadata,cache::make_context_metadata(next)) &&
            !same(original_digest,cache::details::cache_key_hash(next)),"even unused switches distinguish true loader metadata and path material");
    };
    for(::std::size_t index{};index!=members.size();++index)
    {
        auto changed{parameter}; changed.*members[index]=true;
        auto const policy{cp::module_feature_policy::from_actual_parameter(changed)};
        require(policy.disabled==(::std::uint64_t{1u}<<index) && policy.cli_mode==0u && policy.controlled==0u,"nineteen independent disable bits in declared order");
        distinct(policy);
    }
    for(::std::uint64_t mode{1u};mode!=5u;++mode)
    {
        auto changed{parameter}; changed.cli_mode=static_cast<features::wasm_feature_cli_mode>(mode);
        auto const policy{cp::module_feature_policy::from_actual_parameter(changed)};
        require(policy.cli_mode==mode && policy.disabled==0u && policy.controlled==0u,"actual CLI grammar mode copied"); distinct(policy);
    }
    auto controlled{parameter}; controlled.controllable_allow_multi_result_vector=true;
    auto const results{cp::module_feature_policy::from_actual_parameter(controlled)};
    require(results.controlled==1u && (results.compatibility_mask()&1u)==0u,"legacy multi-result restriction preserved"); distinct(results);
    controlled=parameter; controlled.controllable_allow_multi_table=true;
    auto const tables{cp::module_feature_policy::from_actual_parameter(controlled)};
    require(tables.controlled==2u,"legacy multi-table restriction preserved even though the compatibility mask has no separate bit"); distinct(tables);
    auto disabled{parameter}; disabled.disable_gc=true; disabled.disable_threads=true;
    auto const safe_subset{cp::module_feature_policy::from_actual_parameter(disabled)};
    require((safe_subset.compatibility_mask()&((::std::uint64_t{1u}<<12u)|(::std::uint64_t{1u}<<16u)))==0u,"disabled GC/threads are not inferred from product capabilities");
    auto bookkeeping{parameter}; bookkeeping.explicit_enable_gc=true; bookkeeping.explicit_enable_threads=true;
    require(cp::module_feature_policy::from_actual_parameter(bookkeeping)==baseline,"CLI conflict ownership is not executable policy");
    cp::module_feature_policy invalid{5u,0u,0u}; require(!invalid.valid(),"reserved CLI mode fails");
    invalid={0u,::std::uint64_t{1u}<<19u,0u}; require(!invalid.valid(),"reserved disable flag fails");
    invalid={0u,0u,4u}; require(!invalid.valid(),"reserved controllable flag fails");
    ::fast_io::io::println("module feature policy 22 fields DATA PASS; runtime/restore=false");
}

// Component DATA only: no cache files, native code, VM stop or restore.
// Build separately against both products, with and without UWVM_MODULE.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <fast_io.h>
#ifndef UWVM_MODULE
# include <uwvm2/runtime/checkpoint/materialization.h>
# include <uwvm2/runtime/llvm_jit_cache/store.h>
#else
import uwvm2.runtime.checkpoint.materialization;
import uwvm2.runtime.llvm_jit_cache;
#endif
namespace cp = ::uwvm2::runtime::checkpoint;
namespace cache = ::uwvm2::runtime::llvm_jit_cache;
namespace containers = ::uwvm2::utils::container;
static void require(bool valid, char const* message)
{
    if(!valid)
    {
        ::fast_io::io::perrln("checkpoint cache identity: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
template<typename Range>
static bool same(Range const& a, Range const& b)
{ return a.size() == b.size() && ::std::equal(a.cbegin(), a.cend(), b.cbegin()); }
static containers::u8string text(cp::compilation_profile::cache_identity_type const& tuple)
{
    containers::u8string result{};
    containers::u8string_ref_uwvm output{::std::addressof(result)};
    cp::compilation_profile::print_cache_identity_to(output, tuple);
    return result;
}
static cache::cache_context context(containers::u8string const& tuple)
{
    cache::cache_context result{};
    result.cache_key = u8"checkpoint-cache-tuple-component";
    result.target_triple = u8"component-target";
    result.cpu_name = u8"component-cpu";
    result.cpu_features = u8"component-features";
    result.llvm_version = u8"component-llvm";
    result.uwvm_abi = u8"component-runtime-abi";
    result.codegen_policy = cache::details::make_cache_key(u8"llvm-jit-full-codegen-policy");
    cache::details::append_cache_key_value(result.codegen_policy, u8"checkpoint-typed-entry-policy", tuple);
    return result;
}
int main()
{
    // The on-disk cache format stays5. Actual native resume ABI2 and state
    // schema6 change canonical context/path material, isolating old keys.
    static_assert(cache::cache_format_version == 5u);
    auto const profile{cp::compilation_profile::create_for_trusted_manager()};
    require(bool(profile), "actual profile factory");
    auto const tuple{profile->cache_identity()};
    auto const canonical{text(tuple)};
    containers::u8string const expected{containers::u8concat_uwvm(u8"6143850319052752689/",
        ::fast_io::mnp::dec(tuple[1u]), u8"/6/2/16/4096/1048576/65536/65536/32768")};
    require(same(canonical, expected) && tuple[2u] == cp::state_schema_revision && tuple[3u] == 2u, "default tuple has current schema and all ten declared fields");
    auto old_schema{tuple}; old_schema[2u] = 5u;
    require(!same(cache::details::cache_key_hash(context(text(old_schema))), cache::details::cache_key_hash(context(canonical))),
        "previous state schema cannot reuse current checkpoint object-cache context");
    containers::u8string via_instance{};
    containers::u8string_ref_uwvm instance_output{::std::addressof(via_instance)};
    profile->print_cache_identity_to(instance_output);
    require(same(canonical, via_instance), "runtime instance uses the shared serializer");
    auto const base{context(canonical)};
    auto const base_metadata{cache::make_context_metadata(base)};
    auto const base_path{cache::details::cache_key_hash(base)};
    auto const observer{cp::compilation_profile::create_for_trusted_observer()};
    require(bool(observer) && observer->purpose() == cp::compilation_purpose::observe_values &&
        profile->purpose() == cp::compilation_purpose::resumable, "immutable distinct profile purposes");
    auto const observed_tuple{observer->cache_identity()};
    require(observed_tuple[1u] == 13u, "activation-owned heap observer policy invalidates old native packet cache");
    require(observed_tuple.size() == tuple.size() && observed_tuple[1u] != tuple[1u],
        "explicit observation/resumable policy versions differ with identical caps");
    auto const observed{context(text(observed_tuple))};
    require(!same(base_metadata, cache::make_context_metadata(observed)) &&
        !same(base_path, cache::details::cache_key_hash(observed)), "purposes never share cache metadata/path");
    // Modify each copy of scalar DATA, not a real immutable profile. Include
    // the trailing workspace field which the original full call sites omitted.
    for(::std::size_t index{}; index != tuple.size(); ++index)
    {
        auto different{tuple};
        ++different[index];
        auto const changed_text{text(different)};
        auto const changed{context(changed_text)};
        require(!same(canonical, changed_text), "each declared field affects canonical text");
        require(!same(base_metadata, cache::make_context_metadata(changed)), "each field affects loader context comparison");
        require(!same(base_path, cache::details::cache_key_hash(changed)), "each field affects actual path digest");
    }
    auto different_workspace{tuple};
    different_workspace.back() += 1u;
    auto const workspace{context(text(different_workspace))};
    require(!same(base_path, cache::details::cache_key_hash(workspace)), "different native workspace limit cannot share key");
    // Historical nine-field canonical value: no filesystem write or signed
    // object is needed to prove different exact metadata/path identities.
    containers::u8string const old_nine{u8"6143850319052752689/5/3/1/16/4096/1048576/65536/65536"};
    auto const old{context(old_nine)};
    require(!same(base_metadata, cache::make_context_metadata(old)) &&
            !same(base_path, cache::details::cache_key_hash(old)), "legacy truncated tuple cannot match complete identity");
    cp::compilation_profile::cache_identity_type extremes{};
    extremes.back() = (::std::numeric_limits<::std::uint64_t>::max)();
    containers::u8string const expected_extremes{u8"0/0/0/0/0/0/0/0/0/18446744073709551615"};
    require(same(text(extremes), expected_extremes), "uint64 formatting is canonical and lossless");
    ::fast_io::io::println("checkpoint complete cache tuple DATA PASS; executable restore acceptance=false");
}

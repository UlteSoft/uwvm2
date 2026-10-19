// v7 successor: raw fixture codec signatures are strictly inert memory DATA.
// Genuine persistent sync/async/load refusal is separate; no CLI hit is claimed.
// Build separately for each product. The driver supplies identical contexts
// and roots. All positive roundtrips/foreign mutations use bounded inert memory
// DATA; paths are computed for namespace comparison only, never published.
// No generated code or authenticated production object is executed.
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#ifndef UWVM_MODULE
# include <uwvm2/runtime/llvm_jit_cache/environment.h>
# include <uwvm2/runtime/llvm_jit_cache/store.h>
#else
import uwvm2.runtime.llvm_jit_cache;
#endif
#include "llvm_jit_cache_data_test_helpers.h"

namespace cache = ::uwvm2::runtime::llvm_jit_cache;
template <typename String>
static std::string text(String const& s)
{ return {reinterpret_cast<char const*>(s.cbegin()), s.size()}; }

static cache::cache_context context(char const* root, bool signed_blob)
{
    cache::cache_context ctx{};
    ctx.cache_dir = cache::details::u8string_from_cstr(root);
    if(signed_blob) { ctx.cache_key = u8"product-isolation-signed"; }
    else { ctx.cache_key = u8"product-isolation-unsigned"; }
    ctx.target_triple = u8"identical-test-target";
    ctx.cpu_name = u8"identical-test-cpu";
    ctx.cpu_features = u8"identical-test-features";
    ctx.data_layout = u8"identical-test-layout";
    ctx.self_executable_sha256[0u] = std::byte{0x42u};
    ctx.has_self_executable_sha256 = true; // Synthetic DATA, not actual image proof.
    ctx.llvm_version = u8"identical-test-llvm";
    ctx.uwvm_abi = u8"deliberately-identical-embedder-abi";
    ctx.codegen_policy = u8"identical-test-policy";
    ctx.signature_seed = cache::collect_signature_seed(ctx);
    ctx.has_signature_seed = true;
    return ctx;
}

int main(int argc, char** argv)
{
    static_assert(cache::cache_format_version == 7u);
    std::cout << "codec_data_only=1\nnative_cache_replay=0\n";
    if(argc == 1)
    {
        std::cout << "product=" << text(cache::cache_product_name) << "\nversion=" << cache::cache_format_version
                  << "\ndefault=" << text(cache::default_cache_directory())
                  << "\nabi-hex=";
        // Fingerprints are length-prefixed binary keys, not line-oriented
        // text. A length byte can be '\n'; printing it raw let the Python
        // driver silently truncate everything after the early ABI fields.
        constexpr char digits[]{"0123456789abcdef"};
        for(unsigned char byte: text(cache::uwvm_runtime_abi_fingerprint()))
        { std::cout << digits[byte >> 4] << digits[byte & 15]; }
        std::cout << '\n';
        return 0;
    }
    if(argc < 4) { return 2; }
    auto const mode{std::string_view{argv[3]}};
    bool const signed_blob{mode == "signed" || mode == "native-signed"};
    auto ctx{context(argv[2], signed_blob)};
    if(mode == "native-signed" || mode == "native-unsigned")
    {
        ctx.uwvm_abi = cache::uwvm_runtime_abi_fingerprint();
        ctx.signature_seed = cache::collect_signature_seed(ctx);
    }
    bool const old_call_policy{std::string_view{argv[1]} == "policy-v1"};
    bool const old_table_policy{std::string_view{argv[1]} == "table-layout-v12"};
    bool const old_memory_policy{std::string_view{argv[1]} == "memory-layout-v13"};
    bool const old_owner_policy{std::string_view{argv[1]} == "memory-layout-v15"};
    bool const old_shared_type_policy{std::string_view{argv[1]} == "memory-layout-v16"};
    bool const old_atomic_bridge_policy{std::string_view{argv[1]} == "memory-layout-v18"};
    bool const old_wide_table_policy{std::string_view{argv[1]} == "table64-v24"};
    bool const old_function_interval_policy{std::string_view{argv[1]} == "function-calls-v26"};
    bool const old_wide_memory_policy{std::string_view{argv[1]} == "memory64-v23"};
    bool const old_tuple_tail_policy{std::string_view{argv[1]} == "typed-calls-v22"};
    bool const old_unqualified_tail_policy{std::string_view{argv[1]} == "typed-calls-v21"};
    bool const old_typed_call_policy{std::string_view{argv[1]} == "typed-calls-v20"};
    bool const old_atomic_scalar_policy{std::string_view{argv[1]} == "atomic-scalars-v19"};
    bool const old_ordered_size_policy{std::string_view{argv[1]} == "memory-layout-v17"};
    bool const old_hot_metadata_policy{std::string_view{argv[1]} == "memory-layout-v14"};
    if(old_table_policy || old_memory_policy || old_hot_metadata_policy || old_owner_policy || old_shared_type_policy || old_ordered_size_policy || old_atomic_bridge_policy || old_atomic_scalar_policy || old_typed_call_policy || old_unqualified_tail_policy || old_tuple_tail_policy || old_wide_memory_policy || old_wide_table_policy || old_function_interval_policy)
    {
        ctx.uwvm_abi = cache::uwvm_runtime_abi_fingerprint();
        ctx.signature_seed = cache::collect_signature_seed(ctx);
    }
    if(old_call_policy)
    {
        ctx.uwvm_abi = u8"full-width-noabicalls-c-abi-v2";
        ctx.signature_seed = cache::collect_signature_seed(ctx);
    }
    cache::cache_policy policy{};
    // Signed and unsigned fixture DATA use the same namespace/context checks.
    // The production persistent API refuses the unsigned policy separately.
    policy.generate_signature = signed_blob;
    policy.verify_signature = signed_blob;
    policy.compression = cache::compression_kind::none;
    constexpr std::byte payload[]{std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
                                  std::byte{0x89}, std::byte{0xab}, std::byte{0xcd}, std::byte{0xef}};
    if(!cache_data_test::standalone_refuses(ctx)) { return 18; }
    std::cout << "standalone_persistent_refused=1\n";
    cache_data_test::bytes blob{};
    auto const stored{cache::details::build_cache_blob(ctx,payload,sizeof(payload),policy,blob)};
    if(stored != cache::cache_status::ok) { return 3; }
    auto const path{text(cache::cache_file_path(ctx))};
    std::cout << "product=" << text(cache::cache_product_name) << "\npath=" << path << '\n';
    std::cout << "blob-hex=";
    constexpr char hex_digits[]{"0123456789abcdef"};
    for(auto byte:blob){auto value=std::to_integer<unsigned char>(byte);std::cout<<hex_digits[value>>4]<<hex_digits[value&15];}
    std::cout<<'\n';
    auto const own{cache_data_test::decode(ctx,policy,blob)};
    if(own.status != cache::cache_status::ok || own.codec_signature_verified != signed_blob ||
       own.object.size() != sizeof(payload) || !std::equal(own.object.cbegin(), own.object.cend(), payload)) { return 4; }

    if(old_call_policy || old_table_policy || old_memory_policy || old_hot_metadata_policy || old_owner_policy || old_shared_type_policy || old_ordered_size_policy || old_atomic_bridge_policy || old_atomic_scalar_policy || old_typed_call_policy || old_unqualified_tail_policy || old_tuple_tail_policy || old_wide_memory_policy || old_wide_table_policy || old_function_interval_policy)
    {
        // Model an embedder that reuses its cache key, source ID and all other
        // context fields. A raw signed fixture for the old ABI must fail the
        // DATA context comparison after the ABI metadata changes.
        auto previous{ctx};
        if(old_table_policy || old_memory_policy || old_hot_metadata_policy || old_owner_policy || old_shared_type_policy || old_ordered_size_policy || old_atomic_bridge_policy || old_atomic_scalar_policy || old_typed_call_policy || old_unqualified_tail_policy || old_tuple_tail_policy || old_wide_memory_policy || old_wide_table_policy || old_function_interval_policy)
        {
            auto old_abi{text(ctx.uwvm_abi)};
            constexpr std::string_view current{"-runtime-abi-v27"};
            auto const offset{old_abi.find(current)};
            if(offset == std::string::npos) { return 15; }
            auto const previous_version{old_function_interval_policy ? 26 : old_wide_table_policy ? 24 : old_wide_memory_policy ? 23 : old_tuple_tail_policy ? 22 : old_unqualified_tail_policy ? 21 : old_typed_call_policy ? 20 : old_atomic_scalar_policy ? 19 : old_atomic_bridge_policy ? 18 : old_ordered_size_policy ? 17 :
                old_shared_type_policy ? 16 : old_owner_policy ? 15 : old_hot_metadata_policy ? 14 : old_memory_policy ? 13 : 12};
            old_abi.replace(offset, current.size(), std::string{"-runtime-abi-v"} + std::to_string(previous_version));
            previous.uwvm_abi = cache::details::u8string_from_chars(old_abi.data(), old_abi.size());
        }
        else { previous.uwvm_abi = u8"full-width-c-abi-v1"; }
        previous.signature_seed = cache::collect_signature_seed(previous);
        auto const previous_path{text(cache::cache_file_path(previous))};
        // Compare computed paths, then independently compare old fixture DATA
        // under the new metadata. Neither path is read or written.
        cache_data_test::bytes previous_blob{};
        if(previous_path==path||cache::details::build_cache_blob(previous,payload,sizeof(payload),policy,previous_blob)!=cache::cache_status::ok)return 12;
        auto rejected{cache_data_test::decode(ctx,policy,previous_blob)};
        std::cout << "status=" << text(cache::cache_status_name(rejected.status)) << '\n';
        if(rejected.status != cache::cache_status::context_mismatch || !rejected.object.empty() || rejected.codec_signature_verified) { return 13; }
    }
    else if(std::string_view{argv[1]} == "load-foreign")
    {
        if(argc != 7) { return 5; }
        cache_data_test::bytes foreign{};
        if(!cache_data_test::unhex(argv[4],foreign)||foreign.size()<cache::cache_fixed_header_size)return 6;
        auto const mutation{std::string_view{argv[5]}};
        if(mutation == "own-magic")
        {
            for(std::size_t i{}; i != 8; ++i) { foreign[i] = cache::cache_magic[i]; }
        }
        else if(mutation == "version4" || mutation == "version5" || mutation == "version6")
        {
            foreign[8] = static_cast<std::byte>(mutation.back() - '0');foreign[9]=foreign[10]=foreign[11]=std::byte{};
        }
        else if(mutation == "payload") { foreign.back() ^= std::byte{1}; }
        else if(mutation != "none") { return 8; }
        auto rejected{cache_data_test::decode(ctx,policy,foreign)};
        auto const status{text(cache::cache_status_name(rejected.status))};
        std::cout << "status=" << status << '\n';
        if(status != argv[6] || !rejected.object.empty() || rejected.codec_signature_verified) { return 10; }
    }
    else if(std::string_view{argv[1]} != "write") { return 11; }
    return 0;
}

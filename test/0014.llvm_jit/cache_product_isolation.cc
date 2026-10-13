// Build this fixture separately against each product. The driver deliberately
// supplies identical context fields and one shared root; no generated code is
// executed. This tests the real serializer, namespace and loader, including
// rejection after a foreign object's filename/magic have been rewritten.
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#ifndef UWVM_MODULE
# include <uwvm2/runtime/llvm_jit_cache/environment.h>
# include <uwvm2/runtime/llvm_jit_cache/store.h>
#else
import uwvm2.runtime.llvm_jit_cache;
#endif

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
    ctx.llvm_version = u8"identical-test-llvm";
    ctx.uwvm_abi = u8"deliberately-identical-embedder-abi";
    ctx.codegen_policy = u8"identical-test-policy";
    ctx.signature_seed = cache::collect_signature_seed(ctx);
    ctx.has_signature_seed = true;
    return ctx;
}

int main(int argc, char** argv)
{
    static_assert(cache::cache_format_version == 5u);
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
    // ROS's CLI never disables signatures. Exercise the low-level unsigned
    // API too so product separation cannot accidentally depend on that policy.
    policy.generate_signature = signed_blob;
    policy.verify_signature = signed_blob;
    policy.compression = cache::compression_kind::none;
    constexpr std::byte payload[]{std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
                                  std::byte{0x89}, std::byte{0xab}, std::byte{0xcd}, std::byte{0xef}};
    auto const stored{cache::store_object(ctx, payload, sizeof(payload), policy)};
    if(stored != cache::cache_status::ok) { return 3; }
    auto const path{text(cache::cache_file_path(ctx))};
    std::cout << "product=" << text(cache::cache_product_name) << "\npath=" << path << '\n';
    auto const own{cache::load_object(ctx, policy)};
    if(own.status != cache::cache_status::ok || own.signature_verified != signed_blob ||
       own.object.size() != sizeof(payload) || !std::equal(own.object.cbegin(), own.object.cend(), payload)) { return 4; }

    if(old_call_policy || old_table_policy || old_memory_policy || old_hot_metadata_policy || old_owner_policy || old_shared_type_policy || old_ordered_size_policy || old_atomic_bridge_policy || old_atomic_scalar_policy || old_typed_call_policy || old_unqualified_tail_policy || old_tuple_tail_policy || old_wide_memory_policy || old_wide_table_policy || old_function_interval_policy)
    {
        // Model an embedder that reuses its cache key, source ID and all other
        // context fields. Even a legitimately signed old-policy object must
        // not be replayed after the live probe starts using noabicalls.
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
        // Context changes already namespace the filename. Also force the old
        // valid blob into the new filename to exercise the loader's independent
        // context check, rather than accidentally testing only path separation.
        if(previous_path == path || cache::store_object(previous, payload, sizeof(payload), policy) != cache::cache_status::ok) { return 12; }
        ::std::error_code copy_error{};
        ::std::filesystem::copy_file(previous_path, path, ::std::filesystem::copy_options::overwrite_existing, copy_error);
        if(copy_error) { return 14; }
        auto rejected{cache::load_object(ctx, policy)};
        std::cout << "status=" << text(cache::cache_status_name(rejected.status)) << '\n';
        if(rejected.status != cache::cache_status::context_mismatch || !rejected.object.empty() || rejected.signature_verified) { return 13; }
    }
    else if(std::string_view{argv[1]} == "load-foreign")
    {
        if(argc != 7) { return 5; }
        std::ifstream input{argv[4], std::ios::binary};
        if(!input) { return 6; }
        std::vector<char> blob{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        if(blob.size() < cache::cache_fixed_header_size) { return 7; }
        auto const mutation{std::string_view{argv[5]}};
        if(mutation == "own-magic")
        {
            for(std::size_t i{}; i != 8; ++i) { blob[i] = static_cast<char>(cache::cache_magic[i]); }
        }
        else if(mutation == "version4")
        {
            blob[8] = 4; blob[9] = blob[10] = blob[11] = 0;
        }
        else if(mutation == "payload") { blob.back() ^= 1; }
        else if(mutation != "none") { return 8; }
        {
            std::ofstream output{path, std::ios::binary | std::ios::trunc};
            output.write(blob.data(), static_cast<std::streamsize>(blob.size()));
            if(!output) { return 9; }
        }
        auto rejected{cache::load_object(ctx, policy)};
        auto const status{text(cache::cache_status_name(rejected.status))};
        std::cout << "status=" << status << '\n';
        if(status != argv[6] || !rejected.object.empty() || rejected.signature_verified) { return 10; }
    }
    else if(std::string_view{argv[1]} != "write") { return 11; }
    return 0;
}

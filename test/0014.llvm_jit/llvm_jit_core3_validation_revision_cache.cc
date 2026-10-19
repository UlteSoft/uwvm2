// Source-only fixture until a keeper reports an actual build/run receipt.
// No machine code is executed. This uses the actual context encoder, signature
// seed, path digest, actual raw codec builder/parser and DATA context comparison.
#ifndef UWVM_MODULE
# include <uwvm2/runtime/llvm_jit_cache/environment.h>
# include <uwvm2/runtime/llvm_jit_cache/store.h>
#else
import uwvm2.runtime.llvm_jit_cache;
#endif
#include "llvm_jit_cache_data_test_helpers.h"
#include <algorithm>
#include <cstddef>
#include <string_view>
#include <fast_io.h>

namespace cache = ::uwvm2::runtime::llvm_jit_cache;
using string = ::uwvm2::utils::container::u8string;
using view = ::uwvm2::utils::container::u8string_view;
static int failure(unsigned value)
{
    ::fast_io::print(::fast_io::err(), "CORE3_VALIDATION_CACHE failure=", ::fast_io::mnp::dec(value), "\n");
    return 1;
}
int main(int argc, char** argv)
{
    if(argc != 2) { return failure(1u); }
    auto current{cache::default_cache_context(u8"same-original-wasm-and-key", u8"same-codegen-policy")};
    current.cache_dir = cache::details::u8string_from_cstr(argv[1]);
    auto previous{current};
    string encoded_revision{};
    cache::details::append_cache_key_value(encoded_revision, u8"wasm-core3-validation", u8"fused-rich-call-reference-v1");
    ::std::u8string_view const actual{current.uwvm_abi.data(), current.uwvm_abi.size()};
    ::std::u8string_view const field{encoded_revision.data(), encoded_revision.size()};
    auto const offset{actual.find(field)};
    if(offset == ::std::u8string_view::npos || field.empty() || actual.find(field, offset + field.size()) != ::std::u8string_view::npos)
    { return failure(2u); }
    // Reproduce the exact former fingerprint, removing only the new canonical
    // key/value pair. Every product, ABI, source ID, LLVM and target stays equal.
    // [actual begin ... offset ... offset+field.size() ... actual end)
    // [safe: find proved the complete canonical pair lies in actual   ]
    // ^^ substr copies bounded views; fast_io owns every output byte.
    previous.uwvm_abi.clear();
    ::uwvm2::utils::container::u8string_ref_uwvm former{::std::addressof(previous.uwvm_abi)};
    auto const prefix{actual.substr(0uz, offset)}, suffix{actual.substr(offset + field.size())};
    ::fast_io::print(former, view{prefix.data(), prefix.size()}, view{suffix.data(), suffix.size()});
    previous.signature_seed = cache::collect_signature_seed(previous);
    // The current deterministic per-user/ISA/codegen seed deliberately does
    // not include uwvm_abi. Keep it equal here; the raw fixture signature and
    // DATA comparator cover canonical metadata without proving origin.
    if(previous.uwvm_abi == current.uwvm_abi || previous.signature_seed != current.signature_seed) { return failure(3u); }
    auto const former_context{cache::make_context_metadata(previous)};
    auto const actual_context{cache::make_context_metadata(current)};
    if(former_context == actual_context || cache::metadata_equal(former_context.data(), former_context.size(), actual_context))
    { return failure(4u); }
    auto const old_path{cache::cache_file_path(previous)}, new_path{cache::cache_file_path(current)};
    if(old_path == new_path) { return failure(5u); }
    cache::cache_policy policy{};
    // Signed mode is exercised when the build has the real provider. Without
    // it this low-level metadata test uses the unsigned DATA codec only; it
    // does not change ROS or ordinary CLI trust policy or claim signed coverage.
    policy.generate_signature = cache::cache_ed25519_identity_signature_available;
    policy.verify_signature = policy.generate_signature;
    policy.compression = cache::compression_kind::none;
    constexpr ::std::byte object[]{::std::byte{0x14}, ::std::byte{0x82}, ::std::byte{0x01}, ::std::byte{0x15}};
    if(!cache_data_test::standalone_refuses(current)) { return failure(6u); }
    cache_data_test::bytes old_blob{},new_blob{};
    if(cache::details::build_cache_blob(previous,object,sizeof(object),policy,old_blob)!=cache::cache_status::ok ||
       cache::details::build_cache_blob(current,object,sizeof(object),policy,new_blob)!=cache::cache_status::ok)
    { return failure(7u); }
    auto own_old=cache_data_test::decode(previous,policy,old_blob);
    auto foreign=cache_data_test::decode(current,policy,old_blob);
    auto own_new=cache_data_test::decode(current,policy,new_blob);
    if(own_old.status!=cache::cache_status::ok||own_new.status!=cache::cache_status::ok||
       own_old.codec_signature_verified!=policy.generate_signature||own_new.codec_signature_verified!=policy.generate_signature||
       own_old.object.size()!=sizeof(object)||own_new.object.size()!=sizeof(object)||
       !::std::equal(own_old.object.begin(),own_old.object.end(),object)||
       !::std::equal(own_new.object.begin(),own_new.object.end(),object)||
       foreign.status!=cache::cache_status::context_mismatch||!foreign.object.empty()||foreign.codec_signature_verified)
    { return failure(8u); }
    ::fast_io::print(::fast_io::out(), "CORE3_VALIDATION_CACHE codec_data_only=1 standalone_persistent_refused=1 native_cache_replay=0 previous_context_rejected=1 fresh_roundtrip=1 signed=",
        ::fast_io::mnp::dec(static_cast<unsigned>(policy.generate_signature)), "\n");
}

#pragma once
// Test DATA only. Raw fixture signing is not kernel-origin authentication.
// This header never mints a Runtime ticket or changes process dumpability.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
namespace cache_data_test
{
    namespace cache = ::uwvm2::runtime::llvm_jit_cache;
    using bytes = ::uwvm2::utils::container::vector<::std::byte>;
    struct result
    {
        cache::cache_status status{cache::cache_status::malformed};
        bytes object;
        bool codec_signature_verified{};
    };
    template<typename String> inline ::std::string text(String const& s)
    { return {reinterpret_cast<char const*>(s.data()),s.size()}; }

    inline result decode(cache::cache_context const& ctx, cache::cache_policy const& policy, bytes const& blob)
    {
        result out{};cache::cache_blob_view view{};
        // Preserve actual parser behavior, including legacy-format refusal.
        ::std::byte empty{};auto first=blob.empty()?&empty:blob.data();
        out.status=cache::parse_cache_blob(first,first+blob.size(),view);
        if(out.status!=cache::cache_status::ok)return out;
        if(!cache::details::valid_signature_shape(view))
        {out.status=cache::cache_status::malformed;return out;}
        if(view.header.uncompressed_size>policy.max_object_bytes)
        {out.status=cache::cache_status::size_limit_exceeded;return out;}
        auto isa=cache::make_isa_metadata(ctx),metadata=cache::make_context_metadata(ctx);
        if(!cache::metadata_equal(view.isa_metadata,view.header.isa_metadata_size,isa))
        {out.status=cache::cache_status::isa_mismatch;return out;}
        if(!cache::metadata_equal(view.context_metadata,view.header.context_metadata_size,metadata))
        {out.status=cache::cache_status::context_mismatch;return out;}
        auto compression=static_cast<cache::compression_kind>(view.header.compression);
        if(compression!=cache::compression_kind::none&&compression!=cache::compression_kind::uwvm_lzss&&
           compression!=cache::compression_kind::uwvm_native_lz)
        {out.status=cache::cache_status::unsupported_compression;return out;}
        if(policy.verify_signature)
        {
            if(view.header.signature==static_cast<unsigned>(cache::signature_kind::none))
            {out.status=cache::cache_status::signature_missing;return out;}
            if(view.header.signature!=static_cast<unsigned>(cache::signature_kind::ed25519_identity))
            {out.status=cache::cache_status::unsupported_signature;return out;}
            auto header=cache::serialize_fixed_header(view.header);
            if(!cache::details::ed25519_identity_verify(ctx,header,view.isa_metadata,
                static_cast<::std::size_t>(view.header.isa_metadata_size),view.context_metadata,
                static_cast<::std::size_t>(view.header.context_metadata_size),view.signature,view.payload,
                static_cast<::std::size_t>(view.header.payload_size)))
            {out.status=cache::cache_status::signature_mismatch;return out;}
            out.codec_signature_verified=true; // Fixture DATA only, never a cache hit.
        }
        if(!cache::decompress_payload(static_cast<cache::compression_kind>(view.header.compression),view.payload,
            static_cast<::std::size_t>(view.header.payload_size),static_cast<::std::size_t>(view.header.uncompressed_size),out.object))
        {out.object={};out.codec_signature_verified=false;out.status=cache::cache_status::decompression_failed;return out;}
        out.status=cache::cache_status::ok;return out;
    }

    // Strictly bounded inert DATA input from another component executable.
    inline bool unhex(::std::string_view text,bytes& out)
    {
        constexpr ::std::size_t maximum=1048576u;
        if(text.size()%2u||text.size()>2u*maximum)return false;
        auto digit=[](char x)->int {if(x>='0'&&x<='9')return x-'0';if(x>='a'&&x<='f')return x-'a'+10;return -1;};
        out.clear();out.reserve(text.size()/2u);
        for(::std::size_t i{};i!=text.size();i+=2u)
        {auto a=digit(text[i]),b=digit(text[i+1u]);if(a<0||b<0){out.clear();return false;}
         out.push_back(static_cast<::std::byte>((a<<4)|b));}
        return true;
    }

    inline bool standalone_refuses(cache::cache_context ctx)
    {
        ctx.origin.reset(); // Remove, never manufacture, actual Runtime authority.
        auto root=::std::filesystem::path{text(ctx.cache_dir)}/"standalone-persistent-refusal";
        // Save the string owner before converting its actual byte extent.
        auto root_text=root.string();ctx.cache_dir=cache::details::u8string_from_chars(root_text.data(),root_text.size());
        if(::std::filesystem::exists(root))return false;
        constexpr ::std::byte object[]{::std::byte{0x14},::std::byte{0x82}};
        auto allowed=[](cache::cache_status value)
        {return value==cache::cache_status::disabled||value==cache::cache_status::process_not_sealed||
            value==cache::cache_status::origin_admission_unavailable;};
        cache::cache_policy policy{};
        if(!allowed(cache::store_object(ctx,object,sizeof(object),policy))||
           !allowed(cache::store_object_async(ctx,object,sizeof(object),policy)))return false;
        auto loaded=cache::load_object(ctx,policy);
        if(!allowed(loaded.status)||!loaded.object.empty()||loaded.signature_verified)return false;
        policy.generate_signature=false;policy.verify_signature=false;
        if(cache::store_object(ctx,object,sizeof(object),policy)!=cache::cache_status::origin_authentication_required||
           cache::store_object_async(ctx,object,sizeof(object),policy)!=cache::cache_status::origin_authentication_required)return false;
        loaded=cache::load_object(ctx,policy);
        if((loaded.status!=cache::cache_status::disabled&&loaded.status!=cache::cache_status::origin_authentication_required)||
           !loaded.object.empty()||loaded.signature_verified)return false;
        if(!cache::flush_async_store_objects_until(::std::chrono::steady_clock::now()+::std::chrono::seconds{2}))return false;
        return !::std::filesystem::exists(root);
    }
}

// PRIVATE PROPOSAL: typed identity DATA, never a stop/restore capability.
// No current runtime, database or format4 codec selects this header.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
# include <vector>
# include <fast_io.h>
# include <fast_io_crypto.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint::binding
{
    using sha256 = ::std::array<::std::byte, 32u>;
    static_assert(sizeof(::std::size_t) <= sizeof(::std::uint64_t));
    enum class product : ::std::uint32_t { ordinary = 1u, ros = 2u };
    enum class module_role : ::std::uint32_t { main = 1u, dependency = 2u };
    enum class cache_mode : ::std::uint32_t { off = 0u, required_exact = 1u };
    enum class cache_off_reason : ::std::uint32_t
    { none = 0u, explicit_user = 1u, debug_process_binding = 2u, target_process_binding = 3u, source_provenance_unavailable = 4u };
    enum class cache_object_role : ::std::uint32_t { full_module = 1u, full_partition = 2u };
    enum class status : unsigned char
    {
        identical_data, malformed, unknown_identity_revision, unsupported_checkpoint_version,
        unknown_build, incomplete_source_closure, incomplete_cache_bundle, unsupported_cache_version, product_mismatch,
        build_mismatch, compatibility_mismatch, profile_mismatch, checkpoint_body_mismatch,
        wasm_mismatch, replacement_mismatch, cache_mode_mismatch, cache_mismatch,
        quota_exceeded
    };
    inline constexpr ::std::uint32_t identity_revision{2u}, envelope_version{4u}, state_version{6u};
    inline constexpr ::std::uint32_t cache_format_revision{5u};
    inline constexpr ::std::uint32_t canonical_byte_order{1u}; // canonical LE on LE AND BE native hosts
    struct build
    {
        product flavor{product::ordinary};
        ::std::array<::std::uint32_t, 4u> release{}; // diagnostics, never exact build identity alone
        // All six values must be actual private build/provider issuer evidence;
        // arbitrary macro strings, source paths and supplied booleans are not it.
        sha256 compiled_source_manifest{}, compiler_and_link_manifest{}, loaded_product_image{},
            loaded_provider_closure{}, runtime_abi{}, codegen_abi{};
        ::std::uint64_t checkpoint_compatibility_revision{}, continuation_revision{};
        friend bool operator==(build const&, build const&) = default;
    };
    struct module
    {
        ::std::uint64_t ordinal{}, source_bytes{}, role_name_bytes{};
        module_role role{module_role::dependency};
        sha256 original_wasm{}, role_name{};
        // Canonical per-module instance/declaration index/body-generation/body
        // closure, including all live hot replacements. Zero replacements still
        // have a domain-separated digest, not a missing/zero placeholder.
        sha256 effective_function_generation_closure{};
        friend bool operator==(module const&, module const&) = default;
    };
    struct cache_object
    {
        ::std::uint64_t module_ordinal{}, partition_ordinal{}, partition_count{}, blob_bytes{}, object_bytes{};
        cache_object_role role{cache_object_role::full_module};
        ::std::uint32_t format{};
        // Exact accepted cache context/entry, not a predicted key/path, fallback
        // compilation output, test capture object or ObjectCache callback name.
        sha256 complete_key{}, isa_metadata{}, context_metadata{}, stored_blob{}, native_object{}, provider_and_emitter{};
        friend bool operator==(cache_object const&, cache_object const&) = default;
    };
    struct manifest
    {
        ::std::uint32_t revision{identity_revision}, envelope{envelope_version}, state_schema{state_version},
            byte_order{canonical_byte_order};
        build actual_build{};
        ::std::array<::std::uint64_t, 10u> checkpoint_profile{}; // ALL actual profile fields; no text truncation
        sha256 target_and_execution_abi{}, source_link_closure{}, canonical_state_body{}, deterministic_event_prefix{};
        cache_mode caching{cache_mode::off};
        cache_off_reason off_reason{cache_off_reason::explicit_user};
        // Meaning is actual observed completeness, not a restoration credential.
        // Production may only fill these under its private full census producer.
        ::std::vector<module> modules{};
        ::std::vector<cache_object> objects{};
        friend bool operator==(manifest const&, manifest const&) = default;
        [[nodiscard]] constexpr bool grants_restore_authority() const noexcept { return false; }
    };
    struct limits { ::std::size_t modules{4096u}, objects{65536u}; };
    [[nodiscard]] constexpr bool present(sha256 const& hash) noexcept
    { for(auto byte : hash) { if(byte != ::std::byte{}) { return true; } } return false; }
    [[nodiscard]] inline status validate_shape(manifest const& value, limits cap = {}) noexcept
    {
        if(value.revision != identity_revision) { return status::unknown_identity_revision; }
        if(value.envelope != envelope_version || value.state_schema != state_version)
        { return status::unsupported_checkpoint_version; }
        if(value.byte_order != canonical_byte_order || (value.actual_build.flavor != product::ordinary && value.actual_build.flavor != product::ros))
        { return status::malformed; }
        auto const& binary{value.actual_build};
        if(binary.checkpoint_compatibility_revision == 0u || binary.continuation_revision == 0u ||
           !present(binary.compiled_source_manifest) || !present(binary.compiler_and_link_manifest) ||
           !present(binary.loaded_product_image) || !present(binary.loaded_provider_closure) ||
           !present(binary.runtime_abi) || !present(binary.codegen_abi)) { return status::unknown_build; }
        if(!present(value.target_and_execution_abi) || !present(value.source_link_closure) ||
           !present(value.canonical_state_body) || !present(value.deterministic_event_prefix) ||
           value.checkpoint_profile[0u] == 0u || value.checkpoint_profile[1u] == 0u ||
           value.checkpoint_profile[2u] != state_version || value.checkpoint_profile[3u] != binary.continuation_revision ||
           value.checkpoint_profile[4u] == 0u || value.checkpoint_profile[5u] == 0u || value.checkpoint_profile[6u] == 0u ||
           value.checkpoint_profile[7u] == 0u || value.checkpoint_profile[8u] == 0u || value.checkpoint_profile[9u] == 0u)
        { return status::malformed; }
        if(value.modules.empty()) { return status::incomplete_source_closure; }
        if(value.modules.size() > cap.modules || value.objects.size() > cap.objects) { return status::quota_exceeded; }
        ::std::size_t main_count{};
        for(::std::size_t i{}; i != value.modules.size(); ++i)
        {
            // [owned dense source binding vector 0..N] exclusive_end
            // [safe] i<N checked BEFORE selecting this actual DATA record.
            auto const& source{value.modules[i]};
            if(source.ordinal != i || source.source_bytes < 8u ||
               (source.role != module_role::main && source.role != module_role::dependency) ||
               !present(source.original_wasm) || !present(source.role_name) || !present(source.effective_function_generation_closure))
            { return status::incomplete_source_closure; }
            if(source.role == module_role::main) { ++main_count; }
        }
        if(main_count != 1u) { return status::incomplete_source_closure; }
        if(value.caching == cache_mode::off)
        {
            if(!value.objects.empty() || value.off_reason < cache_off_reason::explicit_user ||
               value.off_reason > cache_off_reason::source_provenance_unavailable) { return status::malformed; }
            return status::identical_data;
        }
        if(value.caching != cache_mode::required_exact || value.off_reason != cache_off_reason::none || value.objects.empty())
        { return status::incomplete_cache_bundle; }
        // One complete accepted single object or partition set per dense module.
        // Counts are bounded; verify before increment/subtract/indexing. No
        // omitted module, duplicate slot, reordered set or partial hit is legal.
        ::std::size_t cursor{};
        for(::std::size_t m{}; m != value.modules.size(); ++m)
        {
            if(cursor >= value.objects.size()) { return status::incomplete_cache_bundle; }
            auto const& first{value.objects[cursor]};
            if(first.module_ordinal != m || first.partition_ordinal != 0u || first.partition_count == 0u ||
               first.partition_count > value.objects.size() - cursor ||
               (first.role != cache_object_role::full_module && first.role != cache_object_role::full_partition) ||
               (first.role == cache_object_role::full_module && first.partition_count != 1u))
            { return status::incomplete_cache_bundle; }
            auto const count{static_cast<::std::size_t>(first.partition_count)};
            for(::std::size_t p{}; p != count; ++p)
            {
                // [owned complete bundle: cursor..cursor+count<=N] end
                // [safe] count<=N-cursor above BEFORE cursor+p/indexing.
                auto const& object{value.objects[cursor + p]};
                if(object.format != cache_format_revision) { return status::unsupported_cache_version; }
                if(object.module_ordinal != m || object.partition_ordinal != p || object.partition_count != count ||
                   object.role != first.role || object.blob_bytes < 64u || object.object_bytes == 0u ||
                   !present(object.complete_key) || !present(object.isa_metadata) || !present(object.context_metadata) ||
                   !present(object.stored_blob) || !present(object.native_object) || !present(object.provider_and_emitter))
                { return status::incomplete_cache_bundle; }
            }
            cursor += count; // count<=N-cursor checked BEFORE advance.
        }
        return cursor == value.objects.size() ? status::identical_data : status::incomplete_cache_bundle;
    }
    // Pure bounded DATA comparison. `actual` must eventually come from the
    // private actual source/build/cache producer before ANY VM state read/write.
    // Calling this function on two supplied manifests cannot produce that proof.
    [[nodiscard]] inline status compare(manifest const& saved, manifest const& actual, limits cap = {}) noexcept
    {
        auto const saved_shape{validate_shape(saved, cap)};
        if(saved_shape != status::identical_data) { return saved_shape; }
        auto const actual_shape{validate_shape(actual, cap)};
        if(actual_shape != status::identical_data) { return actual_shape; }
        if(saved.actual_build.flavor != actual.actual_build.flavor) { return status::product_mismatch; }
        if(saved.actual_build != actual.actual_build) { return status::build_mismatch; }
        if(saved.target_and_execution_abi != actual.target_and_execution_abi) { return status::compatibility_mismatch; }
        if(saved.checkpoint_profile != actual.checkpoint_profile) { return status::profile_mismatch; }
        if(saved.canonical_state_body != actual.canonical_state_body || saved.deterministic_event_prefix != actual.deterministic_event_prefix)
        { return status::checkpoint_body_mismatch; }
        if(saved.modules.size() != actual.modules.size() || saved.source_link_closure != actual.source_link_closure) { return status::wasm_mismatch; }
        for(::std::size_t i{}; i != saved.modules.size(); ++i)
        {
            auto const& a{saved.modules[i]}; auto const& b{actual.modules[i]};
            if(a.ordinal != b.ordinal || a.source_bytes != b.source_bytes || a.role_name_bytes != b.role_name_bytes ||
               a.role != b.role || a.original_wasm != b.original_wasm || a.role_name != b.role_name) { return status::wasm_mismatch; }
            if(a.effective_function_generation_closure != b.effective_function_generation_closure) { return status::replacement_mismatch; }
        }
        if(saved.caching != actual.caching || saved.off_reason != actual.off_reason) { return status::cache_mode_mismatch; }
        if(saved.objects != actual.objects) { return status::cache_mismatch; }
        return status::identical_data;
    }
    namespace details
    {
        inline void hash_u32(::fast_io::sha256_context& sha, ::std::uint32_t value) noexcept
        {
            ::std::array<unsigned char, 4u> bytes{};
            // [four owned bytes] end; scratch extent BEFORE cursor formation.
            ::fast_io::basic_obuffer_view<unsigned char> out{bytes.data(), bytes.data() + bytes.size()};
            ::fast_io::print(out, ::fast_io::mnp::le_put<32>(value));
            sha.update(reinterpret_cast<::std::byte const*>(bytes.data()), reinterpret_cast<::std::byte const*>(bytes.data() + bytes.size()));
        }
        inline void hash_u64(::fast_io::sha256_context& sha, ::std::uint64_t value) noexcept
        {
            ::std::array<unsigned char, 8u> bytes{};
            // [eight owned bytes] end; fixed bound precedes all pointer advances.
            ::fast_io::basic_obuffer_view<unsigned char> out{bytes.data(), bytes.data() + bytes.size()};
            ::fast_io::print(out, ::fast_io::mnp::le_put<64>(value));
            sha.update(reinterpret_cast<::std::byte const*>(bytes.data()), reinterpret_cast<::std::byte const*>(bytes.data() + bytes.size()));
        }
        inline void hash_digest(::fast_io::sha256_context& sha, sha256 const& value) noexcept
        {
            // [32 complete owned digest bytes] exclusive_end
            // [safe] fixed std::array extent BEFORE forming the one-past cursor.
            sha.update(value.data(), value.data() + value.size());
        }
    }
    struct hash_result { status result{status::malformed}; sha256 digest{}; };
    [[nodiscard]] inline hash_result hash_bytes(::std::span<::std::byte const> bytes) noexcept
    {
        static_assert(CHAR_BIT == 8);
        if(bytes.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
           (!bytes.empty() && bytes.data() == nullptr)) { return {status::quota_exceeded, {}}; }
        ::fast_io::sha256_context sha{};
        if(!bytes.empty())
        {
            // [actual owner-backed span data..size] end
            // [safe] nonnull and PTRDIFF bound precede cursor advance. A caller
            // span is a host extent contract, NOT immutable ownership authority.
            sha.update(bytes.data(), bytes.data() + bytes.size());
        }
        sha.do_final(); sha256 digest{}; sha.digest_to_byte_ptr(digest.data());
        return {status::identical_data, digest};
    }
    [[nodiscard]] inline hash_result canonical_digest(manifest const& value, limits cap = {}) noexcept
    {
        auto const checked{validate_shape(value, cap)};
        if(checked != status::identical_data) { return {checked, {}}; }
        ::fast_io::sha256_context sha{};
        // Domain bytes and ALL integers are canonical fixed-width LE via fast_io.
        // No native struct layout, pointer, compiler padding or host endianness.
        constexpr ::std::array<::std::byte, 8u> domain{::std::byte{'U'}, ::std::byte{'W'}, ::std::byte{'C'}, ::std::byte{'P'},
            ::std::byte{'I'}, ::std::byte{'D'}, ::std::byte{'0'}, ::std::byte{'2'}};
        // [eight complete owned domain bytes] exclusive_end
        // [safe] fixed array extent precedes the one-past update cursor.
        sha.update(domain.data(), domain.data() + domain.size());
        using namespace details;
        hash_u32(sha, value.revision); hash_u32(sha, value.envelope); hash_u32(sha, value.state_schema); hash_u32(sha, value.byte_order);
        auto const& binary{value.actual_build}; hash_u32(sha, static_cast<::std::uint32_t>(binary.flavor));
        for(auto version : binary.release) { hash_u32(sha, version); }
        hash_digest(sha, binary.compiled_source_manifest); hash_digest(sha, binary.compiler_and_link_manifest);
        hash_digest(sha, binary.loaded_product_image); hash_digest(sha, binary.loaded_provider_closure);
        hash_digest(sha, binary.runtime_abi); hash_digest(sha, binary.codegen_abi);
        hash_u64(sha, binary.checkpoint_compatibility_revision); hash_u64(sha, binary.continuation_revision);
        for(auto field : value.checkpoint_profile) { hash_u64(sha, field); }
        hash_digest(sha, value.target_and_execution_abi); hash_digest(sha, value.source_link_closure);
        hash_digest(sha, value.canonical_state_body); hash_digest(sha, value.deterministic_event_prefix);
        hash_u32(sha, static_cast<::std::uint32_t>(value.caching)); hash_u32(sha, static_cast<::std::uint32_t>(value.off_reason));
        hash_u64(sha, value.modules.size()); hash_u64(sha, value.objects.size());
        for(auto const& source : value.modules)
        {
            hash_u64(sha, source.ordinal); hash_u64(sha, source.source_bytes); hash_u64(sha, source.role_name_bytes);
            hash_u32(sha, static_cast<::std::uint32_t>(source.role));
            hash_digest(sha, source.original_wasm); hash_digest(sha, source.role_name); hash_digest(sha, source.effective_function_generation_closure);
        }
        for(auto const& object : value.objects)
        {
            hash_u64(sha, object.module_ordinal); hash_u64(sha, object.partition_ordinal); hash_u64(sha, object.partition_count);
            hash_u64(sha, object.blob_bytes); hash_u64(sha, object.object_bytes);
            hash_u32(sha, static_cast<::std::uint32_t>(object.role)); hash_u32(sha, object.format);
            hash_digest(sha, object.complete_key); hash_digest(sha, object.isa_metadata); hash_digest(sha, object.context_metadata);
            hash_digest(sha, object.stored_blob); hash_digest(sha, object.native_object); hash_digest(sha, object.provider_and_emitter);
        }
        sha.do_final(); sha256 digest{}; sha.digest_to_byte_ptr(digest.data());
        return {status::identical_data, digest};
    }
}

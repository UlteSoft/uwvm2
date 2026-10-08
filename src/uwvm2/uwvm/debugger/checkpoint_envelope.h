// Identity-bound canonical file DATA. No checkpoint/restore authority is issued.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <string>
# include <utility>
# include <vector>
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/control/owned_file_image.h>
# include "checkpoint_binding.h"
# include "checkpoint_codec.h"
# include "checkpoint_state_identity.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint::envelope
{
    static_assert(CHAR_BIT == 8);
    static_assert(binding::envelope_version == 4u && binding::state_version == database_format_version);
    inline constexpr ::std::uint64_t magic{0x0034424450435755u}, commit{0x34444e4550435755u}; // UWCPDB4\0, UWCPEND4
    inline constexpr ::std::size_t header_size{160u}, footer_size{48u}, identity_fixed_size{484u}, module_size{124u}, cache_object_size{240u};
    enum class status : unsigned char
    {
        decoded_data, legacy_unbound_state_data, truncated, unsupported_version,
        malformed, limit_exceeded, incomplete_commit, digest_mismatch,
        invalid_identity, identity_body_mismatch, identity_source_mismatch, invalid_state, noncanonical_state,
        environment_mismatch, file_read_failed, identity_function_mismatch,
        identity_event_mismatch, identity_state_semantics_unavailable
    };
    struct result
    {
        status observation{status::malformed};
        binding::status identity_status{binding::status::malformed};
        error state_status{error::none};
    };
    struct bounds
    {
        ::std::uint64_t max_file_bytes{256u * 1024u * 1024u};
        ::std::uint64_t max_identity_bytes{32u * 1024u * 1024u};
        binding::limits identities{};
        limits state{};
    };
    // Target environment contains NO current/saved VM state or event body hash.
    // A future private producer must obtain these fields from actual accepted
    // source/build/cache owners. Caller-supplied DATA cannot issue that authority.
    struct target_environment_data
    {
        binding::build actual_build{};
        ::std::array<::std::uint64_t, 10u> checkpoint_profile{};
        binding::sha256 target_and_execution_abi{}, source_link_closure{};
        binding::cache_mode caching{binding::cache_mode::off};
        binding::cache_off_reason off_reason{binding::cache_off_reason::explicit_user};
        ::std::vector<binding::module> modules{};
        ::std::vector<binding::cache_object> objects{};
    };
    struct decoded_file_data
    {
        binding::manifest identity{};
        checkpoint::state saved{};
        friend bool operator==(decoded_file_data const&, decoded_file_data const&) = default;
        [[nodiscard]] constexpr bool grants_restore_authority() const noexcept { return false; }
    };
    namespace details
    {
        [[nodiscard]] inline bool identity_extent(::std::uint64_t modules, ::std::uint64_t objects,
            bounds const& cap, ::std::uint64_t& size) noexcept
        {
            size = identity_fixed_size;
            if(modules == 0u || modules > cap.identities.modules || objects > cap.identities.objects ||
               cap.max_identity_bytes < size || modules > (cap.max_identity_bytes - size) / module_size) { return false; }
            size += modules * module_size; // quotient/subtraction bounds BEFORE multiplication and addition
            if(objects > (cap.max_identity_bytes - size) / cache_object_size) { return false; }
            size += objects * cache_object_size; // remaining quota checked BEFORE multiplication and addition
            return size <= static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)());
        }
        template<typename Output> void put_identity(codec_details::writer<Output>& out, binding::manifest const& id)
        {
            constexpr ::std::array<::std::byte, 8u> domain{::std::byte{'U'},::std::byte{'W'},::std::byte{'C'},::std::byte{'P'},
                ::std::byte{'I'},::std::byte{'D'},::std::byte{'0'},::std::byte{'2'}};
            out.bytes(domain);
            for(auto number : {id.revision, id.envelope, id.state_schema, id.byte_order}) { out.template put<32>(number); }
            out.template put<32>(static_cast<::std::uint32_t>(id.actual_build.flavor));
            for(auto number : id.actual_build.release) { out.template put<32>(number); }
            for(auto const* hash : {::std::addressof(id.actual_build.compiled_source_manifest), ::std::addressof(id.actual_build.compiler_and_link_manifest),
                ::std::addressof(id.actual_build.loaded_product_image), ::std::addressof(id.actual_build.loaded_provider_closure),
                ::std::addressof(id.actual_build.runtime_abi), ::std::addressof(id.actual_build.codegen_abi)}) { out.bytes(*hash); }
            out.template put<64>(id.actual_build.checkpoint_compatibility_revision); out.template put<64>(id.actual_build.continuation_revision);
            for(auto number : id.checkpoint_profile) { out.template put<64>(number); }
            out.bytes(id.target_and_execution_abi); out.bytes(id.source_link_closure); out.bytes(id.canonical_state_body); out.bytes(id.deterministic_event_prefix);
            out.template put<32>(static_cast<::std::uint32_t>(id.caching)); out.template put<32>(static_cast<::std::uint32_t>(id.off_reason));
            out.template put<64>(static_cast<::std::uint64_t>(id.modules.size())); out.template put<64>(static_cast<::std::uint64_t>(id.objects.size()));
            for(auto const& module : id.modules)
            {
                out.template put<64>(module.ordinal); out.template put<64>(module.source_bytes); out.template put<64>(module.role_name_bytes);
                out.template put<32>(static_cast<::std::uint32_t>(module.role)); out.bytes(module.original_wasm); out.bytes(module.role_name);
                out.bytes(module.effective_function_generation_closure);
            }
            for(auto const& object : id.objects)
            {
                for(auto number : {object.module_ordinal, object.partition_ordinal, object.partition_count, object.blob_bytes, object.object_bytes})
                { out.template put<64>(number); }
                out.template put<32>(static_cast<::std::uint32_t>(object.role)); out.template put<32>(object.format);
                out.bytes(object.complete_key); out.bytes(object.isa_metadata); out.bytes(object.context_metadata);
                out.bytes(object.stored_blob); out.bytes(object.native_object); out.bytes(object.provider_and_emitter);
            }
        }
        [[nodiscard]] inline bool get_identity(codec_details::reader& in, ::std::uint64_t module_count,
            ::std::uint64_t object_count, binding::manifest& id)
        {
            ::std::array<::std::byte, 8u> domain{};
            constexpr ::std::array<::std::byte, 8u> expected{::std::byte{'U'},::std::byte{'W'},::std::byte{'C'},::std::byte{'P'},
                ::std::byte{'I'},::std::byte{'D'},::std::byte{'0'},::std::byte{'2'}};
            if(!in.bytes(domain) || domain != expected || !in.get<32>(id.revision) || !in.get<32>(id.envelope) ||
                !in.get<32>(id.state_schema) || !in.get<32>(id.byte_order)) { return false; }
            ::std::uint32_t flavor{}, caching{}, off{};
            if(!in.get<32>(flavor)) { return false; } id.actual_build.flavor = static_cast<binding::product>(flavor);
            for(auto& number : id.actual_build.release) { if(!in.get<32>(number)) { return false; } }
            for(auto* hash : {::std::addressof(id.actual_build.compiled_source_manifest), ::std::addressof(id.actual_build.compiler_and_link_manifest),
                ::std::addressof(id.actual_build.loaded_product_image), ::std::addressof(id.actual_build.loaded_provider_closure),
                ::std::addressof(id.actual_build.runtime_abi), ::std::addressof(id.actual_build.codegen_abi)}) { if(!in.bytes(*hash)) { return false; } }
            if(!in.get<64>(id.actual_build.checkpoint_compatibility_revision) || !in.get<64>(id.actual_build.continuation_revision)) { return false; }
            for(auto& number : id.checkpoint_profile) { if(!in.get<64>(number)) { return false; } }
            if(!in.bytes(id.target_and_execution_abi) || !in.bytes(id.source_link_closure) || !in.bytes(id.canonical_state_body) ||
                !in.bytes(id.deterministic_event_prefix) || !in.get<32>(caching) || !in.get<32>(off)) { return false; }
            id.caching = static_cast<binding::cache_mode>(caching); id.off_reason = static_cast<binding::cache_off_reason>(off);
            ::std::uint64_t modules{}, objects{};
            if(!in.get<64>(modules) || !in.get<64>(objects) || modules != module_count || objects != object_count ||
                modules > in.remaining() / module_size) { return false; }
            // Header/count/complete fixed-width extent preflight has bounded all
            // owner allocations BEFORE these casts, resize, or reader advances.
            id.modules.resize(static_cast<::std::size_t>(modules));
            for(auto& module : id.modules)
            {
                ::std::uint32_t role{};
                if(!in.get<64>(module.ordinal) || !in.get<64>(module.source_bytes) || !in.get<64>(module.role_name_bytes) ||
                    !in.get<32>(role) || !in.bytes(module.original_wasm) || !in.bytes(module.role_name) ||
                    !in.bytes(module.effective_function_generation_closure)) { return false; }
                module.role = static_cast<binding::module_role>(role);
            }
            if(objects > in.remaining() / cache_object_size) { return false; }
            id.objects.resize(static_cast<::std::size_t>(objects));
            for(auto& object : id.objects)
            {
                ::std::uint32_t role{};
                if(!in.get<64>(object.module_ordinal) || !in.get<64>(object.partition_ordinal) || !in.get<64>(object.partition_count) ||
                    !in.get<64>(object.blob_bytes) || !in.get<64>(object.object_bytes) || !in.get<32>(role) || !in.get<32>(object.format) ||
                    !in.bytes(object.complete_key) || !in.bytes(object.isa_metadata) || !in.bytes(object.context_metadata) ||
                    !in.bytes(object.stored_blob) || !in.bytes(object.native_object) || !in.bytes(object.provider_and_emitter)) { return false; }
                object.role = static_cast<binding::cache_object_role>(role);
            }
            return in.remaining() == 0u;
        }
        [[nodiscard]] inline error canonical_state(checkpoint::state const& state, bounds const& cap, ::std::vector<::std::byte>& bytes)
        {
            if(auto valid{validate_graph(state, cap.state)}; valid != error::none) { return valid; }
            ::std::uint64_t payload{};
            if(!codec_details::measure(state, cap.state, payload) || payload > cap.max_file_bytes ||
                cap.max_file_bytes - payload < header_bytes + footer_bytes ||
                payload > static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) - header_bytes - footer_bytes)
            { return error::limit_exceeded; }
            auto const count{static_cast<::std::size_t>(payload + header_bytes + footer_bytes)};
            ::std::vector<::std::byte> candidate(count);
            // [actual owned canonical-state output: count>0 <=PTRDIFF_MAX] end
            // [safe] full measured/validated extent BEFORE first+count.
            auto* first{reinterpret_cast<unsigned char*>(candidate.data())};
            ::fast_io::basic_obuffer_view<unsigned char> out{first, first + count};
            auto const encoded{encode_database(out, state, cap.state)};
            if(encoded != error::none) { return encoded; }
            // [first...first+count] exclusive_end; count checked above before +.
            if(out.curr_ptr != first + count) { return error::malformed; }
            bytes = ::std::move(candidate); return error::none;
        }
        [[nodiscard]] inline binding::status compare_embedded_sources(checkpoint::state const& state,
            binding::manifest const& identity) noexcept
        {
            // Full original Wasm module objects, in increasing file-local object
            // ID order, correspond exactly to the ordered source-role closure.
            // This is byte identity DATA, not parsed type/provider completeness.
            ::std::size_t ordinal{};
            for(auto const& object : state.objects)
            {
                if(object.kind != object_kind::module) { continue; }
                if(ordinal >= identity.modules.size()) { return binding::status::incomplete_source_closure; }
                // [owned identity modules, ordinal<N checked BEFORE indexing]
                auto const& module{identity.modules[ordinal]};
                if(object.bytes.size() != module.source_bytes || binding::hash_bytes(object.bytes).digest != module.original_wasm)
                { return binding::status::wasm_mismatch; }
                ++ordinal; // bounded by actual owned vector size BEFORE advance
            }
            return ordinal == identity.modules.size() ? binding::status::identical_data : binding::status::incomplete_source_closure;
        }
        [[nodiscard]] inline binding::status compare_environment(binding::manifest const& saved,
            target_environment_data const& target, binding::limits cap)
        {
            if(target.modules.size() > cap.modules || target.objects.size() > cap.objects) { return binding::status::quota_exceeded; }
            binding::manifest actual{}; actual.actual_build = target.actual_build; actual.checkpoint_profile = target.checkpoint_profile;
            actual.target_and_execution_abi = target.target_and_execution_abi; actual.source_link_closure = target.source_link_closure;
            actual.caching = target.caching; actual.off_reason = target.off_reason; actual.modules = target.modules; actual.objects = target.objects;
            // These hashes describe the ALREADY verified SAVED body/event prefix,
            // NOT the target's current VM state. The target type has no such fields.
            actual.canonical_state_body = saved.canonical_state_body; actual.deterministic_event_prefix = saved.deterministic_event_prefix;
            return binding::compare(saved, actual, cap);
        }
    }
    [[nodiscard]] inline result check_state_semantics(checkpoint::state const& saved,
        binding::manifest const& identity, bounds const& cap = {}) noexcept
    {
        auto const checked{state_identity::compare(saved, identity, cap.state, cap.identities)};
        switch(checked.observation)
        {
            case state_identity::status::derived_data: return {status::decoded_data, binding::status::identical_data, error::none};
            case state_identity::status::function_closure_mismatch:
                return {status::identity_function_mismatch, binding::status::replacement_mismatch};
            case state_identity::status::event_prefix_mismatch:
                return {status::identity_event_mismatch, binding::status::checkpoint_body_mismatch};
            case state_identity::status::module_count_mismatch:
                return {status::identity_source_mismatch, binding::status::incomplete_source_closure};
            case state_identity::status::limit_exceeded:
                return {status::limit_exceeded, binding::status::quota_exceeded};
            default: return {status::identity_state_semantics_unavailable, binding::status::malformed, checked.graph_error};
        }
    }
    // Encode to an unpublished trusted output only. Caller identity must be an
    // actual full private producer's identity for production; this DATA interface
    // does not attest caller fields or acquire sealed management asset authority.
    template<typename Output> [[nodiscard]] result encode(Output& output, checkpoint::state const& saved,
        binding::manifest const& identity, bounds const& cap = {})
    {
        if(cap.max_file_bytes > static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
        { return {status::limit_exceeded}; }
        auto const shape{binding::validate_shape(identity, cap.identities)};
        if(shape != binding::status::identical_data) { return {status::invalid_identity, shape}; }
        ::std::uint64_t identity_count{};
        if(!details::identity_extent(identity.modules.size(), identity.objects.size(), cap, identity_count)) { return {status::limit_exceeded}; }
        ::std::vector<::std::byte> state_bytes{};
        auto const state_error{details::canonical_state(saved, cap, state_bytes)};
        if(state_error != error::none) { return {status::invalid_state, shape, state_error}; }
        auto const sources{details::compare_embedded_sources(saved, identity)};
        if(sources != binding::status::identical_data) { return {status::identity_source_mismatch, sources}; }
        auto const state_hash{binding::hash_bytes(state_bytes)};
        if(state_hash.result != binding::status::identical_data || state_hash.digest != identity.canonical_state_body)
        { return {status::identity_body_mismatch}; }
        auto const semantics{check_state_semantics(saved, identity, cap)};
        if(semantics.observation != status::decoded_data) { return semantics; }
        if(cap.max_file_bytes < header_size + footer_size || identity_count > cap.max_file_bytes - header_size - footer_size ||
            state_bytes.size() > cap.max_file_bytes - header_size - footer_size - identity_count)
        { return {status::limit_exceeded}; }
        ::std::vector<::std::byte> identity_bytes(static_cast<::std::size_t>(identity_count));
        // [complete fixed measured identity owner, count>=484 <=PTRDIFF] end
        // [safe] count bound BEFORE output cursor/end formation.
        auto* first{reinterpret_cast<unsigned char*>(identity_bytes.data())};
        ::fast_io::basic_obuffer_view<unsigned char> identity_out{first, first + identity_bytes.size()};
        codec_details::writer identity_wire{identity_out}; details::put_identity(identity_wire, identity);
        if(identity_out.curr_ptr != first + identity_bytes.size()) { return {status::malformed}; } // same checked owner bound
        auto const identity_hash{binding::canonical_digest(identity, cap.identities)};
        if(identity_hash.result != binding::status::identical_data || binding::hash_bytes(identity_bytes).digest != identity_hash.digest)
        { return {status::invalid_identity, identity_hash.result}; }
        codec_details::writer wire{output}; wire.template put<64>(magic);
        wire.template put<16>(static_cast<::std::uint16_t>(binding::envelope_version)); wire.template put<16>(static_cast<::std::uint16_t>(header_size));
        wire.template put<32>(::std::uint32_t{0x04030201u}); wire.template put<64>(::std::uint64_t{});
        wire.template put<64>(identity_count); wire.template put<64>(static_cast<::std::uint64_t>(state_bytes.size()));
        wire.template put<64>(static_cast<::std::uint64_t>(identity.modules.size())); wire.template put<64>(static_cast<::std::uint64_t>(identity.objects.size()));
        wire.template put<64>(::std::uint64_t{}); wire.bytes(identity_hash.digest); wire.bytes(state_hash.digest);
        constexpr ::std::array<::std::byte, 32u> reserved{}; wire.bytes(reserved);
        wire.bytes(identity_bytes); wire.bytes(state_bytes); auto const prefix_hash{wire.digest()};
        auto const prefix{header_size + identity_count + state_bytes.size()}; // total subtraction preflight above proves no overflow
        ::fast_io::print(output, ::fast_io::mnp::le_put<64>(commit), ::fast_io::mnp::le_put<64>(prefix));
        // [fixed complete32 prefix hash] end; fixed owned extent BEFORE +32.
        ::fast_io::operations::write_all_bytes(output, prefix_hash.data(), prefix_hash.data() + prefix_hash.size());
        return {status::decoded_data, binding::status::identical_data, error::none};
    }
    // Entire caller span is a genuine live immutable host extent for this call,
    // never arbitrary addresses. File entry below supplies privately copied data,
    // not mmap+fstat/checksum. Failure leaves all prior output DATA unchanged.
    [[nodiscard]] inline result decode(::std::span<::std::byte const> bytes, target_environment_data const& target,
        decoded_file_data& output, bounds const& cap = {})
    {
        if(bytes.size() > cap.max_file_bytes || bytes.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
            (!bytes.empty() && bytes.data() == nullptr)) { return {status::limit_exceeded}; }
        if(bytes.size() < 8u) { return {status::truncated}; }
        codec_details::reader in{bytes}; ::std::uint64_t file_magic{};
        if(!in.get<64>(file_magic)) { return {status::truncated}; }
        if(file_magic == database_magic || file_magic == legacy_database_magic5 || file_magic == legacy_database_magic4 || file_magic == legacy_database_magic3) { return {status::legacy_unbound_state_data}; }
        if(file_magic != magic) { return {status::malformed}; }
        if(bytes.size() < header_size + footer_size) { return {status::truncated}; }
        ::std::uint16_t version{}, header{}; ::std::uint32_t endian{};
        ::std::uint64_t flags{}, identity_count{}, state_count{}, modules{}, objects{}, zero{};
        binding::sha256 identity_hash{}, state_hash{}, reserved{};
        if(!in.get<16>(version) || !in.get<16>(header) || !in.get<32>(endian) || !in.get<64>(flags) || !in.get<64>(identity_count) ||
            !in.get<64>(state_count) || !in.get<64>(modules) || !in.get<64>(objects) || !in.get<64>(zero) ||
            !in.bytes(identity_hash) || !in.bytes(state_hash) || !in.bytes(reserved)) { return {status::truncated}; }
        if(version != binding::envelope_version || header != header_size) { return {status::unsupported_version}; }
        if(endian != 0x04030201u || flags != 0u || zero != 0u || binding::present(reserved)) { return {status::malformed}; }
        ::std::uint64_t measured_identity{};
        if(!details::identity_extent(modules, objects, cap, measured_identity) || identity_count != measured_identity ||
            state_count > cap.state.max_file_bytes) { return {status::limit_exceeded}; }
        auto const payload{bytes.size() - header_size - footer_size}; // minimum total file preflight BEFORE subtraction
        if(identity_count > payload || state_count != payload - identity_count || state_count < header_bytes + footer_bytes)
        { return {status::incomplete_commit}; }
        // [header160][exact measured identity][exact state4][footer48] end
        // [safe] each count<=remaining and whole<=PTRDIFF BEFORE any + below.
        auto const id_count{static_cast<::std::size_t>(identity_count)}, body_count{static_cast<::std::size_t>(state_count)};
        auto const* identity_begin{bytes.data() + header_size};
        auto const* state_begin{identity_begin + id_count};
        auto const* footer_begin{state_begin + body_count};
        codec_details::reader footer{{footer_begin, footer_size}}; ::std::uint64_t committed{}, prefix_count{}; binding::sha256 prefix_hash{};
        if(!footer.get<64>(committed) || !footer.get<64>(prefix_count) || !footer.bytes(prefix_hash) ||
            committed != commit || prefix_count != bytes.size() - footer_size) { return {status::incomplete_commit}; }
        auto const actual_prefix{binding::hash_bytes({bytes.data(), bytes.size() - footer_size})};
        auto const actual_identity{binding::hash_bytes({identity_begin, id_count})};
        auto const actual_state{binding::hash_bytes({state_begin, body_count})};
        if(actual_prefix.digest != prefix_hash || actual_identity.digest != identity_hash || actual_state.digest != state_hash)
        { return {status::digest_mismatch}; }
        decoded_file_data candidate{}; codec_details::reader identity_reader{{identity_begin, id_count}};
        if(!details::get_identity(identity_reader, modules, objects, candidate.identity)) { return {status::invalid_identity}; }
        auto const identity_shape{binding::validate_shape(candidate.identity, cap.identities)};
        if(identity_shape != binding::status::identical_data) { return {status::invalid_identity, identity_shape}; }
        if(binding::canonical_digest(candidate.identity, cap.identities).digest != identity_hash) { return {status::invalid_identity}; }
        if(candidate.identity.canonical_state_body != actual_state.digest) { return {status::identity_body_mismatch}; }
        auto const decoded{decode_database({state_begin, body_count}, candidate.saved, cap.state)};
        if(decoded != error::none) { return {status::invalid_state, identity_shape, decoded}; }
        ::std::vector<::std::byte> canonical{};
        auto const reencoded{details::canonical_state(candidate.saved, cap, canonical)};
        if(reencoded != error::none || canonical.size() != body_count || binding::hash_bytes(canonical).digest != actual_state.digest)
        { return {status::noncanonical_state, identity_shape, reencoded}; }
        auto const sources{details::compare_embedded_sources(candidate.saved, candidate.identity)};
        if(sources != binding::status::identical_data) { return {status::identity_source_mismatch, sources}; }
        auto const semantics{check_state_semantics(candidate.saved, candidate.identity, cap)};
        if(semantics.observation != status::decoded_data) { return semantics; }
        auto const compared{details::compare_environment(candidate.identity, target, cap.identities)};
        if(compared != binding::status::identical_data) { return {status::environment_mismatch, compared}; }
        output = ::std::move(candidate); return {status::decoded_data, compared, error::none};
    }
    struct file_result
    {
        result decoded{};
        ::uwvm2::utils::control::owned_image_error input_error{::uwvm2::utils::control::owned_image_error::none};
        ::fast_io::error native_error{};
    };
    // Trusted native management DATA reader only. Its private read owner survives
    // the complete parse even if the original pathname is later overwritten or
    // truncated. This helper cannot isolate output files from guest FD/preopens,
    // authorize management, save an instance, or publish a restore credential.
    [[nodiscard]] inline file_result load_file(::std::u8string path, target_environment_data const& target,
        decoded_file_data& output, bounds const& cap = {})
    {
        if(cap.max_file_bytes == 0u || cap.max_file_bytes > ::uwvm2::utils::control::owned_file_image::maximum_bytes)
        { return {{status::limit_exceeded}}; }
        auto image{::uwvm2::utils::control::owned_file_image::read(::std::move(path), static_cast<::std::size_t>(cap.max_file_bytes))};
        if(!image) { return {{status::file_read_failed}, image.error, image.native_error}; }
        return {decode(image.image->bytes(), target, output, cap)};
    }
}

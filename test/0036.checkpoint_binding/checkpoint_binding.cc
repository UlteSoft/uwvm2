// Actual owned file/hash and actual signed cache-format component. Never a
// loaded-runtime build issuer, executed-cache receipt, stop or restore proof.
#include <uwvm2/uwvm/debugger/checkpoint_binding_dependency_contract.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/llvm_jit_cache/store.h>
#include <fast_io.h>
#include <fast_io_crypto.h>
#include <fast_io_unit/string.h>
#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <span>
#include <string_view>
namespace bind = ::uwvm2::uwvm::debugger::checkpoint::binding;
namespace cache = ::uwvm2::runtime::llvm_jit_cache;
namespace image = ::uwvm2::utils::control;
static void require(bool valid, unsigned line)
{
    if(valid) { return; }
    ::fast_io::print(::fast_io::err(), "checkpoint_binding FAIL line=", ::fast_io::mnp::dec(line), "\n");
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
static bind::sha256 hash(::std::span<::std::byte const> bytes)
{
    auto result{bind::hash_bytes(bytes)}; REQUIRE(result.result == bind::status::identical_data); return result.digest;
}
static bind::sha256 label(::std::string_view text)
{
    // [actual string_view owner, complete code-unit extent] exclusive_end
    // [safe] reinterpretation only; hash_bytes checks PTRDIFF/null before advance.
    return hash({reinterpret_cast<::std::byte const*>(text.data()), text.size()});
}
static auto load(char const* path)
{
    auto result{image::owned_file_image::read(::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(path))), 1048576uz)};
    REQUIRE(result); return result;
}
static void export_bytes(char const* path, ::std::span<::std::byte const> bytes)
{
    REQUIRE(!bytes.empty() && bytes.size() <= PTRDIFF_MAX);
    ::fast_io::obuf_file out{::fast_io::mnp::os_c_str(path)};
    // [actual owned byte vector, bytes.size()>0 and <=PTRDIFF_MAX] end
    // [safe] checked extent BEFORE forming write_all_bytes one-past cursor.
    ::fast_io::operations::write_all_bytes(out, bytes.data(), bytes.data() + bytes.size()); ::fast_io::flush(out);
}
static auto blob(cache::cache_context const& context, image::owned_file_image const& input)
{
    cache::cache_policy policy{}; policy.compression = cache::compression_kind::none;
    ::uwvm2::utils::container::vector<::std::byte> result{};
    // [actual immutable owner input.bytes().data(), input.size()] exclusive_end
    // [safe] source factory bound <=1MiB/PTRDIFF; actual writer constructs and
    // signs real cache metadata/header/payload, no duplicated test-only codec.
    REQUIRE(cache::details::build_cache_blob(context, input.bytes().data(), input.size(), policy, result) == cache::cache_status::ok);
    cache::cache_blob_view parsed{};
    REQUIRE(cache::parse_cache_blob(result.cbegin(), result.cend(), parsed) == cache::cache_status::ok);
    auto const header{cache::serialize_fixed_header(parsed.header)};
    REQUIRE(cache::details::ed25519_identity_verify(context, header, parsed.isa_metadata,
        static_cast<::std::size_t>(parsed.header.isa_metadata_size), parsed.context_metadata,
        static_cast<::std::size_t>(parsed.header.context_metadata_size), parsed.signature, parsed.payload,
        static_cast<::std::size_t>(parsed.header.payload_size)));
    return result;
}
int main(int argc, char** argv)
{
    // Approved keeper creates modern Wasm A/B with identical code and different
    // custom/data bytes, plus different object-payload files and private outputs.
    if(argc != 11) { return 64; }
    auto const endian{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[10]))};
    if(endian == "--require-big") { REQUIRE(::std::endian::native == ::std::endian::big); }
    else { REQUIRE(endian == "--require-little" && ::std::endian::native == ::std::endian::little); }
    auto wasm_a{load(argv[1])}; auto wasm_b{load(argv[2])}; auto object_a{load(argv[3])}; auto object_b{load(argv[4])};
    auto const wa{hash(wasm_a.image->bytes())}, wb{hash(wasm_b.image->bytes())};
    auto const oa{hash(object_a.image->bytes())}, ob{hash(object_b.image->bytes())};
    REQUIRE(wa != wb && oa != ob && wasm_a.image->size() >= 8u && wasm_b.image->size() >= 8u);
    cache::cache_context ctx{};
    ctx.cache_key = cache::details::make_cache_key(u8"actual-checkpoint-binding-component");
    cache::details::append_cache_key_value(ctx.cache_key, u8"wasm", u8"fixture-A");
    ctx.target_triple = u8"component-target"; ctx.cpu_name = u8"component-cpu"; ctx.cpu_features = u8"component-feature";
    ctx.llvm_version = u8"component-provider"; ctx.uwvm_abi = u8"component-abi"; ctx.codegen_policy = u8"component-policy";
    ctx.signature_seed[0u] = ::std::byte{0x42u}; ctx.has_signature_seed = true; ctx.cache_key_is_complete = true;
    // This test seed is not runtime/cache admission authority. Actual signing
    // and verification establish only this component's codec/context evidence.
    auto a{blob(ctx, *object_a.image)}; auto b{blob(ctx, *object_b.image)};
    export_bytes(argv[5], {a.data(), a.size()}); export_bytes(argv[6], {b.data(), b.size()});
    auto actual_a{load(argv[5])}; auto actual_b{load(argv[6])};
    auto const ca{hash(actual_a.image->bytes())}, cb{hash(actual_b.image->bytes())}; REQUIRE(ca != cb);
    cache::cache_blob_view ca_view{}, cb_view{};
    // [factory-owned whole cache bytes, size>=64 and <=1MiB/PTRDIFF] end
    // [safe] actual writer/footer size checked before each end formation.
    REQUIRE(actual_a.image->size() >= 64u && actual_b.image->size() >= 64u);
    REQUIRE(cache::parse_cache_blob(actual_a.image->bytes().data(), actual_a.image->bytes().data() + actual_a.image->size(), ca_view) == cache::cache_status::ok);
    REQUIRE(cache::parse_cache_blob(actual_b.image->bytes().data(), actual_b.image->bytes().data() + actual_b.image->size(), cb_view) == cache::cache_status::ok);
    // Alter the real cache header through a bounded fast_io LE writer. The
    // cache parser must reject its unsupported version before metadata/payload.
    auto wrong_cache_format{a}; REQUIRE(wrong_cache_format.size() >= 12u);
    // [magic8][version4][remaining...>=0] exclusive_end
    // [safe] complete 12byte bound BEFORE base+8/base+12 cursor formation.
    auto* format_begin{reinterpret_cast<unsigned char*>(wrong_cache_format.data()) + 8u};
    ::fast_io::basic_obuffer_view<unsigned char> format_out{format_begin, format_begin + 4u};
    ::fast_io::print(format_out, ::fast_io::mnp::le_put<32>(cache::cache_format_version - 1u));
    cache::cache_blob_view wrong_view{};
    REQUIRE(cache::parse_cache_blob(wrong_cache_format.cbegin(), wrong_cache_format.cend(), wrong_view) == cache::cache_status::unsupported_version);
    auto cross_product_cache{a}; REQUIRE(cross_product_cache.size() >= 8u);
    // [actual fixed8 other-product magic][owned real cache bytes>=8] end
    // [safe] BOTH fixed source and destination bounds precede byte copy.
    constexpr ::std::array<::std::byte, 8u> ordinary_magic{::std::byte{'U'}, ::std::byte{'W'}, ::std::byte{'V'}, ::std::byte{'M'},
        ::std::byte{'L'}, ::std::byte{'J'}, ::std::byte{'C'}, ::std::byte{1u}};
    constexpr ::std::array<::std::byte, 8u> ros_magic{::std::byte{'U'}, ::std::byte{'W'}, ::std::byte{'V'}, ::std::byte{'M'},
        ::std::byte{'R'}, ::std::byte{'O'}, ::std::byte{'S'}, ::std::byte{1u}};
    auto const& other_magic{cache::cache_product_name.size() == 5u ? ros_magic : ordinary_magic};
    ::fast_io::freestanding::my_memcpy(cross_product_cache.data(), other_magic.data(), other_magic.size());
    REQUIRE(cache::parse_cache_blob(cross_product_cache.cbegin(), cross_product_cache.cend(), wrong_view) == cache::cache_status::invalid_magic);
    auto isa{cache::make_isa_metadata(ctx)}; auto context{cache::make_context_metadata(ctx)};
    bind::manifest original{};
    original.actual_build.flavor = cache::cache_product_name.size() == 5u ? bind::product::ordinary : bind::product::ros;
    original.actual_build.release = {2u, 0u, 4u, 0u};
    original.actual_build.compiled_source_manifest = label("actual component source fixture");
    original.actual_build.compiler_and_link_manifest = label("actual component compile/link fixture");
    original.actual_build.loaded_product_image = oa; original.actual_build.loaded_provider_closure = label("actual component provider fixture");
    original.actual_build.runtime_abi = label("actual component ABI fixture"); original.actual_build.codegen_abi = label("actual component codegen fixture");
    original.actual_build.checkpoint_compatibility_revision = 1u; original.actual_build.continuation_revision = ::uwvm2::runtime::checkpoint::native_resume_abi_revision;
    auto const actual_profile{::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager()};
    REQUIRE(actual_profile); original.checkpoint_profile = actual_profile->cache_identity();
    original.target_and_execution_abi = label("actual component target+execution");
    original.source_link_closure = label("actual component single-source closure");
    original.canonical_state_body = label("actual component state body"); original.deterministic_event_prefix = label("actual component empty events");
    original.caching = bind::cache_mode::required_exact; original.off_reason = bind::cache_off_reason::none;
    original.modules.push_back({0u, wasm_a.image->size(), 4u, bind::module_role::main, wa, label("main"), label("actual component gen1 bodies")});
    original.objects.push_back({0u, 0u, 1u, actual_a.image->size(), object_a.image->size(), bind::cache_object_role::full_module,
        cache::cache_format_version, hash({reinterpret_cast<::std::byte const*>(ctx.cache_key.data()), ctx.cache_key.size()}),
        hash({isa.data(), isa.size()}), hash({context.data(), context.size()}), ca, oa, label("actual component provider/emitter")});
    REQUIRE(bind::validate_shape(original) == bind::status::identical_data && bind::compare(original, original) == bind::status::identical_data);
    REQUIRE(!original.grants_restore_authority());
    ::std::size_t negatives{};
    auto mismatch{[&](auto change, bind::status expected)
    {
        auto changed{original}; change(changed); REQUIRE(bind::compare(original, changed) == expected);
        if(bind::validate_shape(changed) == bind::status::identical_data)
        { REQUIRE(bind::canonical_digest(original).digest != bind::canonical_digest(changed).digest); }
        ++negatives;
    }};
    mismatch([&](auto& m) { m.modules[0u].original_wasm = wb; m.modules[0u].source_bytes = wasm_b.image->size(); }, bind::status::wasm_mismatch);
    mismatch([](auto& m) { ++m.modules[0u].source_bytes; }, bind::status::wasm_mismatch);
    mismatch([](auto& m) { m.modules[0u].role_name = label("other"); }, bind::status::wasm_mismatch);
    mismatch([](auto& m) { m.source_link_closure = label("other closure"); }, bind::status::wasm_mismatch);
    mismatch([](auto& m) { m.modules[0u].effective_function_generation_closure = label("actual component gen2 bodies"); }, bind::status::replacement_mismatch);
    mismatch([&](auto& m) { m.objects[0u].stored_blob = cb; m.objects[0u].native_object = ob;
        m.objects[0u].blob_bytes = actual_b.image->size(); m.objects[0u].object_bytes = object_b.image->size(); }, bind::status::cache_mismatch);
    mismatch([](auto& m) { m.objects[0u].complete_key = label("other actual key"); }, bind::status::cache_mismatch);
    mismatch([](auto& m) { m.objects[0u].isa_metadata = label("other ISA"); }, bind::status::cache_mismatch);
    mismatch([](auto& m) { m.objects[0u].context_metadata = label("other actual context"); }, bind::status::cache_mismatch);
    mismatch([](auto& m) { m.objects[0u].provider_and_emitter = label("other emitter"); }, bind::status::cache_mismatch);
    mismatch([](auto& m) { m.actual_build.flavor = m.actual_build.flavor == bind::product::ordinary ? bind::product::ros : bind::product::ordinary; }, bind::status::product_mismatch);
    // Same displayed semantic version, genuinely different owned fixture bytes.
    // This is DATA comparison, not proof those fixture files are loaded VM code.
    mismatch([&](auto& m) { m.actual_build.loaded_product_image = ob; }, bind::status::build_mismatch);
    mismatch([](auto& m) { m.actual_build.compiled_source_manifest = label("same version changed source"); }, bind::status::build_mismatch);
    mismatch([](auto& m) { m.actual_build.loaded_provider_closure = label("same version patched LLVM"); }, bind::status::build_mismatch);
    mismatch([](auto& m) { m.actual_build.runtime_abi = label("other exact runtime ABI"); }, bind::status::build_mismatch);
    mismatch([](auto& m) { m.target_and_execution_abi = label("other target"); }, bind::status::compatibility_mismatch);
    mismatch([](auto& m) { --m.checkpoint_profile[9u]; }, bind::status::profile_mismatch);
    mismatch([](auto& m) { m.canonical_state_body = label("other checkpoint body"); }, bind::status::checkpoint_body_mismatch);
    mismatch([](auto& m) { ++m.envelope; }, bind::status::unsupported_checkpoint_version);
    mismatch([](auto& m) { --m.state_schema; }, bind::status::unsupported_checkpoint_version);
    mismatch([](auto& m) { ++m.revision; }, bind::status::unknown_identity_revision);
    mismatch([](auto& m) { --m.objects[0u].format; }, bind::status::unsupported_cache_version);
    mismatch([](auto& m) { m.objects.clear(); }, bind::status::incomplete_cache_bundle);
    mismatch([](auto& m) { m.objects[0u].partition_count = 2u; }, bind::status::incomplete_cache_bundle);
    mismatch([](auto& m) { m.modules[0u].ordinal = 1u; }, bind::status::incomplete_source_closure);
    mismatch([](auto& m) { m.actual_build.loaded_product_image = {}; }, bind::status::unknown_build);
    mismatch([](auto& m) { m.caching = bind::cache_mode::off; m.off_reason = bind::cache_off_reason::explicit_user; m.objects.clear(); }, bind::status::cache_mode_mismatch);
    mismatch([](auto& m) { m.actual_build.compiler_and_link_manifest = label("other exact compiler/link closure"); }, bind::status::build_mismatch);
    mismatch([](auto& m) { m.actual_build.codegen_abi = label("other exact codegen ABI"); }, bind::status::build_mismatch);
    mismatch([](auto& m) { m.modules[0u].role = bind::module_role::dependency; }, bind::status::incomplete_source_closure);
    mismatch([](auto& m) { ++m.modules[0u].role_name_bytes; }, bind::status::wasm_mismatch);
    auto off{original}; off.caching = bind::cache_mode::off; off.off_reason = bind::cache_off_reason::explicit_user; off.objects.clear();
    auto other_reason{off}; other_reason.off_reason = bind::cache_off_reason::debug_process_binding;
    REQUIRE(bind::compare(off, other_reason) == bind::status::cache_mode_mismatch); ++negatives;
    auto incomplete{original}; incomplete.modules.push_back(original.modules[0u]); incomplete.modules[1u].ordinal = 1u;
    incomplete.modules[1u].role = bind::module_role::dependency;
    REQUIRE(bind::validate_shape(incomplete) == bind::status::incomplete_cache_bundle); ++negatives;
    REQUIRE(bind::validate_shape(original, {0u, 0u}) == bind::status::quota_exceeded); ++negatives;
    // Both source modules and every actual signed cache blob are DATA fixtures.
    // These are complete ordered shape/identity positives, not loaded-engine
    // acceptance receipts. The real cache writer/verifier produced each blob.
    auto complete_full{original};
    complete_full.source_link_closure = label("actual component two-source closure");
    complete_full.modules.push_back({1u, wasm_b.image->size(), 10u, bind::module_role::dependency,
        wb, label("dependency"), label("actual component dependency gen1 bodies")});
    auto second_full{original.objects[0u]}; second_full.module_ordinal = 1u;
    second_full.blob_bytes = actual_b.image->size(); second_full.object_bytes = object_b.image->size();
    second_full.stored_blob = cb; second_full.native_object = ob;
    complete_full.objects.push_back(second_full);
    REQUIRE(bind::validate_shape(complete_full) == bind::status::identical_data);
    REQUIRE(bind::compare(complete_full, complete_full) == bind::status::identical_data);
    auto partition_context{ctx};
    cache::details::append_cache_key_value(partition_context.cache_key, u8"module-ordinal", u8"0");
    cache::details::append_cache_key_value(partition_context.cache_key, u8"partition-count", u8"2");
    cache::details::append_cache_key_value(partition_context.cache_key, u8"partition-ordinal", u8"0");
    auto partition_context1{ctx};
    cache::details::append_cache_key_value(partition_context1.cache_key, u8"module-ordinal", u8"0");
    cache::details::append_cache_key_value(partition_context1.cache_key, u8"partition-count", u8"2");
    cache::details::append_cache_key_value(partition_context1.cache_key, u8"partition-ordinal", u8"1");
    auto full_context1{ctx};
    cache::details::append_cache_key_value(full_context1.cache_key, u8"module-ordinal", u8"1");
    auto p0{blob(partition_context, *object_a.image)}, p1{blob(partition_context1, *object_b.image)}, f1{blob(full_context1, *object_b.image)};
    export_bytes(argv[7], {p0.data(), p0.size()}); export_bytes(argv[8], {p1.data(), p1.size()});
    export_bytes(argv[9], {f1.data(), f1.size()});
    auto canonical_object{[&](cache::cache_context const& actual_context, auto const& actual_blob, auto const& actual_payload,
        ::std::uint64_t module, ::std::uint64_t partition, ::std::uint64_t count, bind::cache_object_role role)
    {
        auto const actual_isa{cache::make_isa_metadata(actual_context)}, actual_metadata{cache::make_context_metadata(actual_context)};
        return bind::cache_object{module, partition, count, actual_blob.size(), actual_payload.size(), role,
            cache::cache_format_version,
            hash({reinterpret_cast<::std::byte const*>(actual_context.cache_key.data()), actual_context.cache_key.size()}),
            hash({actual_isa.data(), actual_isa.size()}), hash({actual_metadata.data(), actual_metadata.size()}),
            hash({actual_blob.data(), actual_blob.size()}), hash(actual_payload.bytes()), label("actual component provider/emitter")};
    }};
    auto complete_partitioned{complete_full}; complete_partitioned.objects.clear();
    complete_partitioned.objects.push_back(canonical_object(partition_context, p0, *object_a.image, 0u, 0u, 2u, bind::cache_object_role::full_partition));
    complete_partitioned.objects.push_back(canonical_object(partition_context1, p1, *object_b.image, 0u, 1u, 2u, bind::cache_object_role::full_partition));
    complete_partitioned.objects.push_back(canonical_object(full_context1, f1, *object_b.image, 1u, 0u, 1u, bind::cache_object_role::full_module));
    REQUIRE(bind::validate_shape(complete_partitioned) == bind::status::identical_data);
    REQUIRE(bind::compare(complete_partitioned, complete_partitioned) == bind::status::identical_data);
    auto bundle_mismatch{[&](auto change, bind::status expected)
    {
        auto changed{complete_partitioned}; change(changed);
        REQUIRE(bind::compare(complete_partitioned, changed) == expected);
        if(bind::validate_shape(changed) == bind::status::identical_data)
        { REQUIRE(bind::canonical_digest(changed).digest != bind::canonical_digest(complete_partitioned).digest); }
        ++negatives;
    }};
    bundle_mismatch([](auto& m) { ::std::swap(m.modules[0u], m.modules[1u]); }, bind::status::incomplete_source_closure);
    bundle_mismatch([](auto& m) { ::std::swap(m.modules[0u].original_wasm, m.modules[1u].original_wasm); }, bind::status::wasm_mismatch);
    bundle_mismatch([](auto& m) { ::std::swap(m.objects[0u], m.objects[1u]); }, bind::status::incomplete_cache_bundle);
    bundle_mismatch([&](auto& m) { m.objects[1u].native_object = oa; }, bind::status::cache_mismatch);
    bundle_mismatch([](auto& m) { ::std::swap(m.objects[0u], m.objects[2u]); }, bind::status::incomplete_cache_bundle);
    bundle_mismatch([](auto& m)
    {
        // [complete known fixture 3-element object owner] end
        // [safe] size==3 precondition BEFORE forming begin()+1 for erase.
        REQUIRE(m.objects.size() == 3u); m.objects.erase(m.objects.begin() + 1u);
    }, bind::status::incomplete_cache_bundle);
    bundle_mismatch([](auto& m) { m.objects[1u].partition_ordinal = 0u; }, bind::status::incomplete_cache_bundle);
    bundle_mismatch([](auto& m) { m.objects.push_back(m.objects[2u]); }, bind::status::incomplete_cache_bundle);
    bundle_mismatch([](auto& m) { --m.objects[1u].format; }, bind::status::unsupported_cache_version);
    bundle_mismatch([](auto& m) { m.objects[2u].role = bind::cache_object_role::full_partition; }, bind::status::cache_mismatch);
    bundle_mismatch([](auto& m) { ++m.modules[1u].role_name_bytes; }, bind::status::wasm_mismatch);
    auto const bundle_digest{bind::canonical_digest(complete_partitioned)};
    REQUIRE(bundle_digest.result == bind::status::identical_data);
    auto const digest{bind::canonical_digest(original)}; REQUIRE(digest.result == bind::status::identical_data);
    ::fast_io::print(::fast_io::out(), "CHECKPOINT_BINDING endian=",
        ::fast_io::mnp::os_c_str(::std::endian::native == ::std::endian::big ? "big" : "little"), " negatives=",
        ::fast_io::mnp::dec(negatives), " digest=");
    for(auto byte : digest.digest) { ::fast_io::print(::fast_io::out(), ::fast_io::mnp::hex<false, true>(::std::to_integer<unsigned char>(byte))); }
    ::fast_io::print(::fast_io::out(), " bundle_digest=");
    for(auto byte : bundle_digest.digest) { ::fast_io::print(::fast_io::out(), ::fast_io::mnp::hex<false, true>(::std::to_integer<unsigned char>(byte))); }
    ::fast_io::print(::fast_io::out(), " complete_modules=2 complete_objects=3 canonical_data_only=1 loaded_build_cache_issuer=0 whole_restore=0\n");
}

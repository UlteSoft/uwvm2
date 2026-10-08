// Structural Core3 DATA / genuine file & signed-cache codec test; not a VM checkpoint.
#include <uwvm2/uwvm/debugger/checkpoint_envelope.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/llvm_jit_cache/store.h>
#include <fast_io_unit/string.h>
#include <algorithm>
#include <bit>
#include <string_view>
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
namespace env = cp::envelope;
namespace bind = cp::binding;
namespace cache = ::uwvm2::runtime::llvm_jit_cache;
namespace image = ::uwvm2::utils::control;
static void require(bool value, unsigned line)
{ if(!value) { ::fast_io::io::perrln("checkpoint envelope FAIL line=", ::fast_io::mnp::dec(line)); ::fast_io::fast_terminate(); } }
#define REQUIRE(x) require(bool(x), __LINE__)
static auto hash(::std::span<::std::byte const> bytes)
{ auto value{bind::hash_bytes(bytes)}; REQUIRE(value.result == bind::status::identical_data); return value.digest; }
static auto label(::std::string_view text)
{ return hash({reinterpret_cast<::std::byte const*>(text.data()), text.size()}); }
static auto path(char const* text)
{ return ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(text))); }
static auto load(char const* name)
{ auto data{image::owned_file_image::read(path(name), 1048576uz)}; REQUIRE(data); return data; }
static void write_file(char const* name, ::std::span<::std::byte const> bytes)
{
    REQUIRE(!bytes.empty() && bytes.size() <= PTRDIFF_MAX);
    ::fast_io::obuf_file file{::fast_io::mnp::os_c_str(name), ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::excl,
        static_cast<::fast_io::perms>(0600u)};
    // [actual owner span, 0<size<=PTRDIFF checked BEFORE first+size] end
    ::fast_io::operations::write_all_bytes(file, bytes.data(), bytes.data() + bytes.size()); ::fast_io::flush(file); file.close();
}
static cp::value number(cp::value_kind kind, ::std::uint64_t low, ::std::uint64_t high = 0u)
{ cp::value result{}; result.type.kind = kind; result.low_bits = low; result.high_bits = high; return result; }
static cp::value reference(cp::heap_kind heap, cp::reference_kind kind, cp::object_id target = 0u)
{
    cp::value result{}; result.type.kind = cp::value_kind::reference; result.type.heap = heap;
    result.type.nullable = true; result.reference = kind; result.target = target;
    if(heap == cp::heap_kind::defined) { result.type.type_module = 1u; result.type.type_index = 0u; }
    return result;
}
static cp::state sample()
{
    cp::state result{}; result.recording_id[0] = ::std::byte{0x80u}; result.recording_id[15] = ::std::byte{1u};
    result.checkpoint_id = 2u; result.parent_checkpoint_id = 1u; result.logical_instruction = 200u;
    result.required_features = cp::known_feature_mask; result.next_logical_thread = 2u;
    result.objects.resize(27u);
    auto item = [&](::std::size_t id, cp::object_kind kind) -> cp::object&
    { REQUIRE(id != 0u && id <= result.objects.size()); auto& object{result.objects[id - 1u]}; object.kind = kind; return object; };
    auto& module{item(1u, cp::object_kind::module)};
    module.bytes = {::std::byte{}, ::std::byte{'a'}, ::std::byte{'s'}, ::std::byte{'m'}, ::std::byte{1u}, ::std::byte{}, ::std::byte{}, ::std::byte{}};
    auto& instance{item(2u, cp::object_kind::instance)};
    instance.words = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u}; instance.links = {1u, 3u, 6u, 4u, 8u, 9u, 10u, 11u};
    auto& function{item(3u, cp::object_kind::function)}; function.words[1] = 7u; function.words[2] = 3u; function.links = {2u};
    function.bytes = {::std::byte{}, ::std::byte{0x41u}, ::std::byte{1u}, ::std::byte{0x0bu}};
    auto& memory{item(4u, cp::object_kind::memory)};
    memory.flags = 1u; memory.words = {64u, ::std::uint64_t{1u} << 48u, 0u, ::std::uint64_t{1u} << 48u};
    auto& memory_chunk{item(5u, cp::object_kind::memory_chunk)};
    memory_chunk.links = {4u}; memory_chunk.words[0] = (::std::numeric_limits<::std::uint64_t>::max)() - 3u;
    memory_chunk.bytes = {::std::byte{1u}, ::std::byte{2u}, ::std::byte{3u}, ::std::byte{4u}};
    auto const node{reference(cp::heap_kind::defined, cp::reference_kind::structure, 12u)};
    auto& table{item(6u, cp::object_kind::table)};
    table.words = {64u, 0x100000001u, 0u, (::std::numeric_limits<::std::uint64_t>::max)()};
    table.values = {reference(cp::heap_kind::defined, cp::reference_kind::null)};
    auto& table_chunk{item(7u, cp::object_kind::table_chunk)}; table_chunk.links = {6u}; table_chunk.words[0] = 0x100000000u; table_chunk.values = {node};
    auto& global{item(8u, cp::object_kind::global)}; global.flags = 1u; global.values = {node};
    auto& tag{item(9u, cp::object_kind::tag)}; tag.links = {1u}; tag.words[0] = 2u;
    item(10u, cp::object_kind::data).bytes = {::std::byte{}, ::std::byte{0xffu}, ::std::byte{1u}};
    item(11u, cp::object_kind::element).values = {reference(cp::heap_kind::func, cp::reference_kind::function, 3u)};
    auto& structure{item(12u, cp::object_kind::structure)}; structure.links = {1u}; structure.values = {node, number(cp::value_kind::i8, 255u)};
    auto& array{item(13u, cp::object_kind::array)}; array.links = {1u}; array.words[0] = 1u; array.values = {node, node};
    auto& exception{item(14u, cp::object_kind::exception)};
    exception.links = {9u, 23u}; exception.values = {node, number(cp::value_kind::i64, (::std::numeric_limits<::std::uint64_t>::max)())};
    auto& external{item(15u, cp::object_kind::external)}; external.values = {reference(cp::heap_kind::any, cp::reference_kind::host, 24u)};
    auto& thread{item(16u, cp::object_kind::thread)}; thread.flags = 2u; thread.words[0] = 1u; thread.words[1] = 1u; thread.links = {17u, 20u};
    auto& frame{item(17u, cp::object_kind::frame)}; frame.links = {3u, 18u, 19u}; frame.words = {123u, 6u, 3u, 1u, 1u, 7u, 0u, 0u};
    auto i31{reference(cp::heap_kind::i31, cp::reference_kind::i31)}; i31.low_bits = 0x7fffffffu;
    auto unset{reference(cp::heap_kind::defined, cp::reference_kind::null)}; unset.type.nullable = false; unset.initialized = false;
    frame.values = {node, i31, number(cp::value_kind::v128, 0x0807060504030201u, 0x100f0e0d0c0b0a09u),
        reference(cp::heap_kind::external, cp::reference_kind::external, 15u), reference(cp::heap_kind::exception, cp::reference_kind::exception, 14u), unset,
        number(cp::value_kind::i32, 0xffffffffu), number(cp::value_kind::f32, 0x7fc01234u), number(cp::value_kind::f64, 0x8000000000000000u)};
    auto& control{item(18u, cp::object_kind::control)}; control.words = {100u, 200u, 0u, 1u};
    auto& handler{item(19u, cp::object_kind::handler)}; handler.flags = 1u; handler.links = {9u}; handler.words = {0u, 333u, 14u};
    auto& wait{item(20u, cp::object_kind::atomic_wait)};
    wait.links = {4u}; wait.words = {(::std::numeric_limits<::std::uint64_t>::max)() - 3u, 4u, 0u, 999u};
    auto& host{item(21u, cp::object_kind::host_resource)}; host.flags = 2u; host.words = {42u, 1u, 1u}; host.bytes = {::std::byte{0x7fu}};
    auto& event{item(22u, cp::object_kind::event)}; event.flags = 2u; event.links = {21u};
    event.words = {1u, 100u, 1u, 7u, 1u, 1u, 0u, 5u}; event.values = {number(cp::value_kind::i32, 4u), number(cp::value_kind::i32, 111u)};
    event.bytes = {::std::byte{0xdeu}, ::std::byte{0xadu}};
    auto& trace{item(23u, cp::object_kind::exception_trace)}; trace.links = {3u}; trace.values = {number(cp::value_kind::i64, 123u)};
    auto& host_reference{item(24u, cp::object_kind::host_reference)};
    host_reference.links = {21u}; host_reference.words[0] = 1u; host_reference.bytes = {::std::byte{0x5au}};
    item(25u, cp::object_kind::external).values = {node};
    item(26u, cp::object_kind::external).values = {i31};
    item(27u, cp::object_kind::external).values = {reference(cp::heap_kind::array, cp::reference_kind::array, 13u)};
    result.root_instances = {2u}; result.retained_roots = {node, reference(cp::heap_kind::exception, cp::reference_kind::exception, 14u)};
    return result;
}
static auto target(bind::manifest const& id)
{
    env::target_environment_data result{}; result.actual_build = id.actual_build; result.checkpoint_profile = id.checkpoint_profile;
    result.target_and_execution_abi = id.target_and_execution_abi; result.source_link_closure = id.source_link_closure;
    result.caching = id.caching; result.off_reason = id.off_reason; result.modules = id.modules; result.objects = id.objects; return result;
}
static auto encode(cp::state const& state, bind::manifest const& id)
{
    ::std::vector<::std::byte> bytes(65536u);
    // [exact owned64KiB output] end; full owner bound BEFORE cursor formation.
    auto* first{reinterpret_cast<unsigned char*>(bytes.data())}; ::fast_io::basic_obuffer_view<unsigned char> out{first, first + bytes.size()};
    REQUIRE(env::encode(out, state, id).observation == env::status::decoded_data);
    REQUIRE(out.curr_ptr >= first && static_cast<::std::size_t>(out.curr_ptr - first) <= bytes.size());
    bytes.resize(static_cast<::std::size_t>(out.curr_ptr - first)); return bytes;
}
static auto state_bytes(cp::state const& state)
{ ::std::vector<::std::byte> bytes{}; REQUIRE(env::details::canonical_state(state, {}, bytes) == cp::error::none); return bytes; }
template<unsigned Bits, typename T> static void overwrite(::std::vector<::std::byte>& bytes, ::std::size_t offset, T value)
{
    REQUIRE(offset <= bytes.size() && Bits / 8u <= bytes.size() - offset);
    // [owned bytes][offset: exact Bits/8 mutation] end
    // [safe] both remaining length and offset checked BEFORE +offset/+width.
    auto* first{reinterpret_cast<unsigned char*>(bytes.data()) + offset};
    ::fast_io::basic_obuffer_view<unsigned char> out{first, first + Bits / 8u}; ::fast_io::print(out, ::fast_io::mnp::le_put<Bits>(value));
}
static void put_hash(::std::vector<::std::byte>& bytes, ::std::size_t offset, bind::sha256 const& value)
{
    REQUIRE(offset <= bytes.size() && value.size() <= bytes.size() - offset);
    // [complete32 actual hash] -> [owned offset...offset+32] end
    // [safe] source/destination extent checked BEFORE destination pointer advance.
    ::fast_io::freestanding::my_memcpy(bytes.data() + offset, value.data(), value.size());
}
static void resign(::std::vector<::std::byte>& bytes, ::std::size_t identity_count, bool repair_nested_state = false)
{
    REQUIRE(bytes.size() >= env::header_size + env::footer_size && identity_count >= env::identity_fixed_size &&
        identity_count <= bytes.size() - env::header_size - env::footer_size);
    auto const begin{env::header_size + identity_count}, count{bytes.size() - env::footer_size - begin};
    REQUIRE(count >= cp::header_bytes + cp::footer_bytes);
    // [header][identity][state count>=176][footer] end
    // [safe] all offsets<=remaining BEFORE forming each checked span/copy.
    if(repair_nested_state) { put_hash(bytes, begin + count - 32u, hash({bytes.data() + begin, count - cp::footer_bytes})); }
    auto const body_hash{hash({bytes.data() + begin, count})};
    put_hash(bytes, env::header_size + 396u, body_hash); // identity.canonical_state_body; fixed identity>=484 proved above
    put_hash(bytes, 96u, body_hash);
    put_hash(bytes, 64u, hash({bytes.data() + env::header_size, identity_count}));
    put_hash(bytes, bytes.size() - 32u, hash({bytes.data(), bytes.size() - env::footer_size}));
}
static bind::cache_object actual_object(cache::cache_context const& context, image::owned_file_image const& blob,
    image::owned_file_image const& payload, ::std::uint64_t module, ::std::uint64_t ordinal, ::std::uint64_t count, bind::cache_object_role role)
{
    REQUIRE(blob.size() >= 64u && blob.size() <= PTRDIFF_MAX);
    cache::cache_blob_view parsed{};
    // [actual privately read cache owner, complete checked size] end
    // [safe] bound checked BEFORE data()+size(); real parser checks all internal extents.
    REQUIRE(cache::parse_cache_blob(blob.bytes().data(), blob.bytes().data() + blob.size(), parsed) == cache::cache_status::ok);
    REQUIRE(parsed.header.compression==0u && parsed.header.signature==1u && parsed.header.signature_size==64u);
    auto const header{cache::serialize_fixed_header(parsed.header)};
    REQUIRE(cache::details::ed25519_identity_verify(context, header, parsed.isa_metadata, static_cast<::std::size_t>(parsed.header.isa_metadata_size),
        parsed.context_metadata, static_cast<::std::size_t>(parsed.header.context_metadata_size), parsed.signature, parsed.payload, static_cast<::std::size_t>(parsed.header.payload_size)));
    auto isa{cache::make_isa_metadata(context)}, metadata{cache::make_context_metadata(context)};
    REQUIRE(hash({parsed.isa_metadata, static_cast<::std::size_t>(parsed.header.isa_metadata_size)}) == hash({isa.data(), isa.size()}));
    REQUIRE(hash({parsed.context_metadata, static_cast<::std::size_t>(parsed.header.context_metadata_size)}) == hash({metadata.data(), metadata.size()}));
    REQUIRE(parsed.header.uncompressed_size == payload.size() && parsed.header.payload_size == payload.size());
    REQUIRE(hash({parsed.payload, static_cast<::std::size_t>(parsed.header.payload_size)}) == hash(payload.bytes()));
    return {module, ordinal, count, blob.size(), payload.size(), role, cache::cache_format_version,
        hash({reinterpret_cast<::std::byte const*>(context.cache_key.data()), context.cache_key.size()}), hash({isa.data(), isa.size()}),
        hash({metadata.data(), metadata.size()}), hash(blob.bytes()), hash(payload.bytes()), label("actual component provider/emitter")};
}
int main(int argc, char** argv)
{
    if(argc != 13) { return 64; }
    auto const selector{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[12]))};
    REQUIRE((selector == "--require-little" && ::std::endian::native == ::std::endian::little) ||
        (selector == "--require-big" && ::std::endian::native == ::std::endian::big));
    auto wa{load(argv[1])}, wb{load(argv[2])}, wc{load(argv[3])}, oa{load(argv[4])}, ob{load(argv[5])}, p0{load(argv[6])}, p1{load(argv[7])}, f1{load(argv[8])};
    REQUIRE(hash(wa.image->bytes()) != hash(wb.image->bytes()) && hash(wa.image->bytes()) != hash(wc.image->bytes()) && hash(oa.image->bytes()) != hash(ob.image->bytes()));
    cache::cache_context base{}; base.cache_key = cache::details::make_cache_key(u8"actual-checkpoint-binding-component");
    cache::details::append_cache_key_value(base.cache_key, u8"wasm", u8"fixture-A");
    base.target_triple=u8"component-target"; base.cpu_name=u8"component-cpu"; base.cpu_features=u8"component-feature";
    base.llvm_version=u8"component-provider"; base.uwvm_abi=u8"component-abi"; base.codegen_policy=u8"component-policy";
    base.signature_seed[0u]=::std::byte{0x42u}; base.has_signature_seed=true; base.cache_key_is_complete=true;
    auto c0{base}, c1{base}, c2{base};
    for(auto* context : {&c0,&c1}) { cache::details::append_cache_key_value(context->cache_key,u8"module-ordinal",u8"0");
        cache::details::append_cache_key_value(context->cache_key,u8"partition-count",u8"2"); }
    cache::details::append_cache_key_value(c0.cache_key,u8"partition-ordinal",u8"0");
    cache::details::append_cache_key_value(c1.cache_key,u8"partition-ordinal",u8"1");
    cache::details::append_cache_key_value(c2.cache_key,u8"module-ordinal",u8"1");
    auto state{sample()}; state.objects[0u].bytes.assign(wa.image->bytes().begin(),wa.image->bytes().end());
    cp::object second_module{}; second_module.kind=cp::object_kind::module;
    second_module.bytes.assign(wb.image->bytes().begin(),wb.image->bytes().end()); state.objects.push_back(::std::move(second_module));
    REQUIRE(cp::validate_graph(state) == cp::error::none);
    bind::manifest id{}; id.actual_build.flavor = cache::cache_product_name.size()==5u ? bind::product::ordinary : bind::product::ros;
    id.actual_build.release={2u,0u,4u,0u}; id.actual_build.compiled_source_manifest=label("actual component source fixture");
    id.actual_build.compiler_and_link_manifest=label("actual component compile/link fixture"); id.actual_build.loaded_product_image=hash(oa.image->bytes());
    id.actual_build.loaded_provider_closure=label("actual component provider fixture"); id.actual_build.runtime_abi=label("actual component ABI fixture");
    id.actual_build.codegen_abi=label("actual component codegen fixture"); id.actual_build.checkpoint_compatibility_revision=1u; id.actual_build.continuation_revision=::uwvm2::runtime::checkpoint::native_resume_abi_revision;
    auto profile{::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager()}; REQUIRE(profile); id.checkpoint_profile=profile->cache_identity();
    id.target_and_execution_abi=label("actual component target+execution"); id.source_link_closure=label("actual component two-source closure");
    id.canonical_state_body=hash(state_bytes(state)); id.deterministic_event_prefix=label("actual component recorded event prefix DATA");
    id.caching=bind::cache_mode::required_exact; id.off_reason=bind::cache_off_reason::none;
    id.modules.push_back({0u,wa.image->size(),4u,bind::module_role::main,hash(wa.image->bytes()),label("main"),label("actual component gen1 bodies")});
    id.modules.push_back({1u,wb.image->size(),10u,bind::module_role::dependency,hash(wb.image->bytes()),label("dependency"),label("actual component dependency gen1 bodies")});
    id.objects={actual_object(c0,*p0.image,*oa.image,0u,0u,2u,bind::cache_object_role::full_partition),
        actual_object(c1,*p1.image,*ob.image,0u,1u,2u,bind::cache_object_role::full_partition),
        actual_object(c2,*f1.image,*ob.image,1u,0u,1u,bind::cache_object_role::full_module)};
    auto const semantics{cp::state_identity::derive(state)};
    REQUIRE(semantics.observation==cp::state_identity::status::derived_data && semantics.functions.size()==id.modules.size());
    for(::std::size_t i{};i!=id.modules.size();++i) { id.modules[i].effective_function_generation_closure=semantics.functions[i]; }
    id.deterministic_event_prefix=semantics.events;
    REQUIRE(bind::validate_shape(id)==bind::status::identical_data); auto actual{target(id)};
    auto bytes{encode(state,id)}; env::decoded_file_data decoded{};
    REQUIRE(env::decode(bytes,actual,decoded).observation==env::status::decoded_data && decoded.saved==state && decoded.identity==id);
    REQUIRE(!decoded.grants_restore_authority() && !decoded.identity.grants_restore_authority());
    auto second_state{state}; ++second_state.logical_instruction; auto second_id{id}; second_id.canonical_state_body=hash(state_bytes(second_state));
    auto second{encode(second_state,second_id)};
    REQUIRE(env::decode(second,actual,decoded).observation==env::status::decoded_data && decoded.saved==second_state);
    // SAME target environment accepts two different verified saved bodies. No
    // target.current_state_hash exists, so restoring an older body is not denied.
    auto off{id}; off.caching=bind::cache_mode::off; off.off_reason=bind::cache_off_reason::debug_process_binding; off.objects.clear();
    auto off_bytes{encode(state,off)};
    REQUIRE(env::decode(off_bytes,target(off),decoded).observation==env::status::decoded_data);
    ::std::size_t negatives{};
    auto mismatch{[&](auto change,bind::status expected)
    { auto wrong{actual}; change(wrong); auto before{decoded}; auto result{env::decode(bytes,wrong,decoded)};
      REQUIRE(result.observation==env::status::environment_mismatch && result.identity_status==expected && decoded==before); ++negatives; }};
    mismatch([&](auto& x){x.modules[0].original_wasm=hash(wb.image->bytes());x.modules[0].source_bytes=wb.image->size();},bind::status::wasm_mismatch);
    mismatch([&](auto& x){x.modules[0].original_wasm=hash(wc.image->bytes());x.modules[0].source_bytes=wc.image->size();},bind::status::wasm_mismatch); // same code/data, different custom section
    mismatch([](auto& x){++x.modules[0].source_bytes;},bind::status::wasm_mismatch);
    mismatch([](auto& x){x.actual_build.flavor=x.actual_build.flavor==bind::product::ordinary?bind::product::ros:bind::product::ordinary;},bind::status::product_mismatch);
    mismatch([](auto& x){x.actual_build.loaded_product_image=label("same version different actual build DATA");},bind::status::build_mismatch);
    for(auto member : {&bind::build::compiled_source_manifest,&bind::build::compiler_and_link_manifest,&bind::build::loaded_provider_closure,&bind::build::runtime_abi,&bind::build::codegen_abi})
    { mismatch([&](auto& x){x.actual_build.*member=label("different build closure DATA");},bind::status::build_mismatch); }
    mismatch([](auto& x){--x.checkpoint_profile[9];},bind::status::profile_mismatch);
    mismatch([](auto& x){x.target_and_execution_abi=label("different execution ABI");},bind::status::compatibility_mismatch);
    mismatch([](auto& x){x.source_link_closure=label("different module closure");},bind::status::wasm_mismatch);
    mismatch([](auto& x){x.modules[1].effective_function_generation_closure=label("different replacement");},bind::status::replacement_mismatch);
    for(::std::size_t i{};i!=3u;++i) { mismatch([&](auto& x){x.objects[i].stored_blob=label("other exact signed blob DATA");},bind::status::cache_mismatch); }
    mismatch([](auto& x){::std::swap(x.objects[0],x.objects[1]);},bind::status::incomplete_cache_bundle);
    mismatch([](auto& x){x.objects.pop_back();},bind::status::incomplete_cache_bundle);
    mismatch([](auto& x){x.objects[1].partition_ordinal=0u;},bind::status::incomplete_cache_bundle);
    mismatch([](auto& x){--x.objects[2].format;},bind::status::unsupported_cache_version);
    mismatch([](auto& x){x.caching=bind::cache_mode::off;x.off_reason=bind::cache_off_reason::debug_process_binding;x.objects.clear();},bind::status::cache_mode_mismatch);
    {auto before{decoded}; REQUIRE(env::decode(off_bytes,actual,decoded).observation==env::status::environment_mismatch && decoded==before); ++negatives;}
    {auto wrong{target(off)};wrong.off_reason=bind::cache_off_reason::explicit_user;auto before{decoded};
     REQUIRE(env::decode(off_bytes,wrong,decoded).identity_status==bind::status::cache_mode_mismatch && decoded==before);++negatives;}
    auto rejected{[&](::std::vector<::std::byte> const& malformed,env::status expected)
    {auto before{decoded}; REQUIRE(env::decode(malformed,actual,decoded).observation==expected && decoded==before);++negatives;}};
    for(::std::size_t i{};i!=bytes.size();++i)
    {auto before{decoded}; REQUIRE(env::decode(::std::span<::std::byte const>{bytes}.first(i),actual,decoded).observation!=env::status::decoded_data && decoded==before);}
    auto broken{bytes}; overwrite<16>(broken,8u,::std::uint16_t{3u}); rejected(broken,env::status::unsupported_version);
    broken=bytes;overwrite<64>(broken,40u,(::std::numeric_limits<::std::uint64_t>::max)()); rejected(broken,env::status::limit_exceeded);
    broken=bytes;overwrite<64>(broken,48u,(::std::numeric_limits<::std::uint64_t>::max)()); rejected(broken,env::status::limit_exceeded);
    broken=bytes;overwrite<64>(broken,24u,(::std::numeric_limits<::std::uint64_t>::max)()); rejected(broken,env::status::limit_exceeded);
    broken=bytes;overwrite<64>(broken,32u,(::std::numeric_limits<::std::uint64_t>::max)()); rejected(broken,env::status::limit_exceeded);
    auto const identity_count{env::identity_fixed_size+2u*env::module_size+3u*env::cache_object_size};
    broken=bytes;broken[env::header_size+20u]^=::std::byte{1}; rejected(broken,env::status::digest_mismatch);
    broken=bytes;overwrite<32>(broken,env::header_size+8u,::std::uint32_t{2u});resign(broken,identity_count);rejected(broken,env::status::invalid_identity);
    broken=bytes;overwrite<32>(broken,env::header_size+16u,::std::uint32_t{2u});resign(broken,identity_count);rejected(broken,env::status::invalid_identity);
    broken=bytes;overwrite<64>(broken,bytes.size()-env::footer_size+8u,::std::uint64_t{});rejected(broken,env::status::incomplete_commit);
    broken=bytes;broken[env::header_size+identity_count+cp::header_bytes+10u]^=::std::byte{1}; rejected(broken,env::status::digest_mismatch);
    broken=bytes;overwrite<16>(broken,env::header_size+identity_count+cp::header_bytes+8u+2u*cp::value_bytes,::std::uint16_t{0xffffu});
    resign(broken,identity_count,true);rejected(broken,env::status::invalid_state); // attacker recomputes ALL hashes; semantic rejection remains
    REQUIRE(wa.image->size()==wc.image->size());
    broken=bytes; auto const wasm_payload{env::header_size+identity_count+cp::header_bytes+8u+2u*cp::value_bytes+cp::object_header_bytes};
    REQUIRE(wasm_payload<=broken.size() && wc.image->size()<=broken.size()-wasm_payload);
    // [private custom-only Wasm C owner] -> [existing module A exact same width]
    // [safe] both extents checked BEFORE destination+offset; repair ALL hashes.
    ::fast_io::freestanding::my_memcpy(broken.data()+wasm_payload,wc.image->bytes().data(),wc.image->size());
    resign(broken,identity_count,true);rejected(broken,env::status::identity_source_mismatch);
    auto legacy{state_bytes(state)};rejected(legacy,env::status::legacy_unbound_state_data);
    cp::state legacy_data{}; REQUIRE(cp::decode_database(legacy,legacy_data)==cp::error::none && legacy_data==state);
    {auto old_state3{legacy}; overwrite<64>(old_state3,0u,cp::legacy_database_magic3);
     auto before{decoded}; REQUIRE(env::decode(old_state3,actual,decoded).observation==env::status::legacy_unbound_state_data && decoded==before);
     REQUIRE(cp::decode_database(old_state3,legacy_data)==cp::error::unsupported_version && legacy_data==state);}
    {auto old_state5{legacy}; overwrite<64>(old_state5,0u,cp::legacy_database_magic5);
     auto before{decoded}; REQUIRE(env::decode(old_state5,actual,decoded).observation==env::status::legacy_unbound_state_data && decoded==before);
     REQUIRE(cp::decode_database(old_state5,legacy_data)==cp::error::unsupported_version && legacy_data==state);}
    {auto stale{id};stale.state_schema=5u;stale.checkpoint_profile[2u]=5u;
     REQUIRE(bind::validate_shape(stale)==bind::status::unsupported_checkpoint_version);}
    {auto stale_profile{id};stale_profile.checkpoint_profile[2u]=5u;
     REQUIRE(bind::validate_shape(stale_profile)==bind::status::malformed);}
    {auto stale{id};stale.state_schema=3u;stale.checkpoint_profile[2u]=3u;
     REQUIRE(bind::validate_shape(stale)==bind::status::unsupported_checkpoint_version);}
    {auto stale_profile{id};stale_profile.checkpoint_profile[2u]=3u;
     REQUIRE(bind::validate_shape(stale_profile)==bind::status::malformed);}
    {env::bounds small{};small.max_identity_bytes=env::identity_fixed_size-1u;auto before{decoded};
     REQUIRE(env::decode(bytes,actual,decoded,small).observation==env::status::limit_exceeded && decoded==before);++negatives;}
    {auto wrong{id};wrong.canonical_state_body=label("wrong saved state");::std::array<unsigned char,32> scratch{};
     ::fast_io::basic_obuffer_view<unsigned char> out{scratch.data(),scratch.data()+scratch.size()};
     REQUIRE(env::encode(out,state,wrong).observation==env::status::identity_body_mismatch && out.curr_ptr==scratch.data());++negatives;}
    {auto wrong_state{state};wrong_state.objects[0u].bytes.assign(wc.image->bytes().begin(),wc.image->bytes().end());
     auto wrong_id{id};wrong_id.canonical_state_body=hash(state_bytes(wrong_state));::std::array<unsigned char,32> scratch{};
     ::fast_io::basic_obuffer_view<unsigned char> out{scratch.data(),scratch.data()+scratch.size()};
     REQUIRE(env::encode(out,wrong_state,wrong_id).observation==env::status::identity_source_mismatch && out.curr_ptr==scratch.data());++negatives;}
    write_file(argv[9],bytes);write_file(argv[10],second);write_file(argv[11],bytes);
    REQUIRE(env::load_file(path(argv[9]),actual,decoded).decoded.observation==env::status::decoded_data && decoded.saved==state);
    auto stable{load(argv[11])};
    {::fast_io::obuf_file truncate{::fast_io::mnp::os_c_str(argv[11]),::fast_io::open_mode::out|::fast_io::open_mode::trunc};::fast_io::flush(truncate);truncate.close();}
    REQUIRE(env::decode(stable.image->bytes(),actual,decoded).observation==env::status::decoded_data && decoded.saved==state);
    auto before{decoded};REQUIRE(env::load_file(path(argv[11]),actual,decoded).decoded.observation==env::status::file_read_failed && decoded==before);
    ::fast_io::io::print("CHECKPOINT_ENVELOPE endian=",::fast_io::mnp::os_c_str(::std::endian::native==::std::endian::big?"big":"little"),
        " negatives=",::fast_io::mnp::dec(negatives)," truncations=",::fast_io::mnp::dec(bytes.size())," bytes=",::fast_io::mnp::dec(bytes.size())," file_sha=");
    for(auto byte:hash(bytes)){::fast_io::io::print(::fast_io::mnp::hex<false,true>(::std::to_integer<unsigned char>(byte)));}
    ::fast_io::io::println(" two_distinct_saved_bodies_one_target=1 canonical_data_only=1 loaded_build_cache_issuer=0 whole_restore=0");
}

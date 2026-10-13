// Real envelope DATA corruption regressions; no VM/restore/native authority.
#include <uwvm2/uwvm/debugger/checkpoint_envelope.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <fast_io_unit/string.h>
#include <bit>
#include <string_view>
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
namespace id = cp::state_identity;
namespace env = cp::envelope;
namespace bind = cp::binding;
static void require(bool value, unsigned line)
{ if(!value) { ::fast_io::io::perrln("checkpoint state identity FAIL line=", ::fast_io::mnp::dec(line)); ::fast_io::fast_terminate(); } }
#define REQUIRE(x) require(bool(x), __LINE__)
static bind::sha256 label(::std::string_view value)
{ return bind::hash_bytes({reinterpret_cast<::std::byte const*>(value.data()),value.size()}).digest; }
static cp::value numeric(::std::uint32_t value)
{ cp::value out{}; out.low_bits=value; return out; }
static cp::state sample()
{
    cp::state out{}; out.recording_id[0u]=::std::byte{0xa5u}; out.checkpoint_id=1u;
    out.logical_instruction=20u; out.replay_event_cursor=1u; out.objects.resize(7u); out.root_instances={2u};
    auto& module{out.objects[0u]}; module.kind=cp::object_kind::module;
    module.bytes={::std::byte{},::std::byte{'a'},::std::byte{'s'},::std::byte{'m'},::std::byte{1u},::std::byte{},::std::byte{},::std::byte{}};
    auto& instance{out.objects[1u]}; instance.kind=cp::object_kind::instance; instance.words[0u]=2u; instance.links={1u,3u,4u};
    for(::std::size_t i{};i!=2u;++i)
    {
        auto& function{out.objects[2u+i]}; function.kind=cp::object_kind::function;
        function.words={i,i+2u,0u}; function.links={2u};
        function.bytes={::std::byte{},::std::byte{0x41u},static_cast<::std::byte>(i+1u),::std::byte{0x0bu}};
    }
    auto& resource{out.objects[4u]}; resource.kind=cp::object_kind::host_resource; resource.flags=1u; resource.words={42u,1u,1u};
    for(::std::size_t i{};i!=2u;++i)
    {
        auto& event{out.objects[5u+i]}; event.kind=cp::object_kind::event; event.flags=2u;
        event.words={i+1u,10u+i,1u,7u,1u,1u,0u,i+1u}; event.links={5u};
        event.values={numeric(10u+static_cast<::std::uint32_t>(i)),numeric(20u+static_cast<::std::uint32_t>(i))};
        event.bytes={static_cast<::std::byte>(0xa0u+i)};
    }
    REQUIRE(cp::validate_graph(out)==cp::error::none); return out;
}
static auto canonical_bytes(cp::state const& state)
{ ::std::vector<::std::byte> out{}; REQUIRE(env::details::canonical_state(state,{},out)==cp::error::none); return out; }
static bind::manifest identity(cp::state const& state)
{
    auto const actual{id::derive(state)}; REQUIRE(actual.observation==id::status::derived_data && actual.functions.size()==1u);
    bind::manifest out{};
    out.actual_build.release={2u,0u,4u,0u};
    for(auto field : {&bind::build::compiled_source_manifest,&bind::build::compiler_and_link_manifest,&bind::build::loaded_product_image,
        &bind::build::loaded_provider_closure,&bind::build::runtime_abi,&bind::build::codegen_abi})
    { out.actual_build.*field=label("component DATA only, never a loaded build issuer"); }
    out.actual_build.checkpoint_compatibility_revision=1u; out.actual_build.continuation_revision=1u;
    out.checkpoint_profile=::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager()->cache_identity();
    out.target_and_execution_abi=label("component target DATA"); out.source_link_closure=label("component source linkage DATA");
    out.canonical_state_body=bind::hash_bytes(canonical_bytes(state)).digest; out.deterministic_event_prefix=actual.events;
    out.modules.push_back({0u,state.objects[0u].bytes.size(),4u,bind::module_role::main,
        bind::hash_bytes(state.objects[0u].bytes).digest,label("main"),actual.functions[0u]});
    REQUIRE(bind::validate_shape(out)==bind::status::identical_data); return out;
}
static auto target(bind::manifest const& saved)
{
    env::target_environment_data out{}; out.actual_build=saved.actual_build; out.checkpoint_profile=saved.checkpoint_profile;
    out.target_and_execution_abi=saved.target_and_execution_abi; out.source_link_closure=saved.source_link_closure;
    out.caching=saved.caching; out.off_reason=saved.off_reason; out.modules=saved.modules; out.objects=saved.objects; return out;
}
static auto encode(cp::state const& state, bind::manifest const& saved)
{
    ::std::vector<::std::byte> bytes(65536u);
    // [owned complete64KiB] end; fixed extent BEFORE output cursor creation.
    auto* first{reinterpret_cast<unsigned char*>(bytes.data())}; ::fast_io::basic_obuffer_view<unsigned char> out{first,first+bytes.size()};
    REQUIRE(env::encode(out,state,saved).observation==env::status::decoded_data);
    REQUIRE(out.curr_ptr>=first && static_cast<::std::size_t>(out.curr_ptr-first)<=bytes.size());
    bytes.resize(static_cast<::std::size_t>(out.curr_ptr-first)); return bytes;
}
static void overwrite_hash(::std::vector<::std::byte>& bytes, ::std::size_t offset, bind::sha256 const& hash)
{
    REQUIRE(offset<=bytes.size() && hash.size()<=bytes.size()-offset);
    // [fixed32 hash] -> [actual bytes offset..offset+32] exclusive_end
    // [safe] both complete extents checked BEFORE +offset/byte copy.
    ::fast_io::freestanding::my_memcpy(bytes.data()+offset,hash.data(),hash.size());
}
static void repair_outer_hashes(::std::vector<::std::byte>& bytes)
{
    constexpr auto extent{env::identity_fixed_size+env::module_size};
    REQUIRE(bytes.size()>=env::header_size+extent+env::footer_size);
    // [header160][identity608][state][footer48] end
    // [safe] total minimum BEFORE +header/subtraction/span construction.
    overwrite_hash(bytes,64u,bind::hash_bytes({bytes.data()+env::header_size,extent}).digest);
    overwrite_hash(bytes,bytes.size()-32u,bind::hash_bytes({bytes.data(),bytes.size()-env::footer_size}).digest);
}
int main(int argc, char** argv)
{
    if(argc==2)
    {
        auto const requested{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[1]))};
        if(requested=="--require-big") { REQUIRE(::std::endian::native==::std::endian::big); }
        else { REQUIRE(requested=="--require-little" && ::std::endian::native==::std::endian::little); }
    }
    else { REQUIRE(argc==1); }
    auto const state{sample()}; auto const saved{identity(state)}; auto const expected{target(saved)};
    auto const derived{id::derive(state)}; REQUIRE(!derived.grants_restore_authority());
    // Independent Python canonical preimage constants, not this C++ encoder.
    REQUIRE((derived.functions[0u]==bind::sha256{::std::byte{0xceu},::std::byte{0x8au},::std::byte{0x9cu},::std::byte{0xbfu},::std::byte{0xc7u},::std::byte{0xd6u},::std::byte{0x8eu},::std::byte{0x12u},::std::byte{0x88u},::std::byte{0x5bu},::std::byte{0xc8u},::std::byte{0x81u},::std::byte{0xaau},::std::byte{0xdfu},::std::byte{0xceu},::std::byte{0xa5u},::std::byte{0x6fu},::std::byte{0x85u},::std::byte{0x26u},::std::byte{0x74u},::std::byte{0x1fu},::std::byte{0x46u},::std::byte{0x29u},::std::byte{0x33u},::std::byte{0x52u},::std::byte{0x0du},::std::byte{0x8fu},::std::byte{0xffu},::std::byte{0xf3u},::std::byte{0x7du},::std::byte{0x21u},::std::byte{0x4cu}}));
    REQUIRE((derived.events==bind::sha256{::std::byte{0x99u},::std::byte{0xfau},::std::byte{0x61u},::std::byte{0x2cu},::std::byte{0x28u},::std::byte{0xb5u},::std::byte{0x5cu},::std::byte{0x5du},::std::byte{0xbau},::std::byte{0x7du},::std::byte{0x6eu},::std::byte{0xf4u},::std::byte{0x4du},::std::byte{0xf0u},::std::byte{0xafu},::std::byte{0xa3u},::std::byte{0x8fu},::std::byte{0x24u},::std::byte{0x88u},::std::byte{0xd9u},::std::byte{0x96u},::std::byte{0xf7u},::std::byte{0x29u},::std::byte{0x36u},::std::byte{0x46u},::std::byte{0xe3u},::std::byte{0x98u},::std::byte{0x9du},::std::byte{0xcbu},::std::byte{0xb6u},::std::byte{0x1cu},::std::byte{0x5fu}}));
    auto const bytes{encode(state,saved)}; env::decoded_file_data decoded{};
    REQUIRE(env::decode(bytes,expected,decoded).observation==env::status::decoded_data && decoded.saved==state);
    REQUIRE(!decoded.grants_restore_authority() && !decoded.identity.grants_restore_authority());
    ::std::size_t negatives{};
    auto reject{[&](auto mutate, bool code)
    {
        auto changed{state}; mutate(changed); REQUIRE(cp::validate_graph(changed)==cp::error::none);
        auto changed_id{identity(changed)}; auto forged{encode(changed,changed_id)};
        // Repair ALL file/checksum layers while retaining the claimed ORIGINAL
        // semantic identity. A checksum-only reader would accept this forgery.
        if(code) { overwrite_hash(forged,env::header_size+env::identity_fixed_size+92u,saved.modules[0u].effective_function_generation_closure); }
        else { overwrite_hash(forged,env::header_size+428u,saved.deterministic_event_prefix); }
        repair_outer_hashes(forged); auto const before{decoded}; auto const result{env::decode(forged,expected,decoded)};
        REQUIRE(result.observation==(code?env::status::identity_function_mismatch:env::status::identity_event_mismatch) && decoded==before);
        auto wrong_id{saved}; wrong_id.canonical_state_body=changed_id.canonical_state_body;
        ::std::array<unsigned char,32u> scratch{};
        ::fast_io::basic_obuffer_view<unsigned char> output{scratch.data(),scratch.data()+scratch.size()};
        REQUIRE(env::encode(output,changed,wrong_id).observation==(code?env::status::identity_function_mismatch:env::status::identity_event_mismatch));
        REQUIRE(output.curr_ptr==scratch.data()); ++negatives;
    }};
    reject([](auto& s){::std::swap(s.objects[2u].bytes,s.objects[3u].bytes);},true);
    reject([](auto& s){++s.objects[2u].words[1u];},true);
    reject([](auto& s){++s.objects[2u].words[2u];},true);
    reject([](auto& s){s.objects[2u].bytes[2u]^=::std::byte{1u};},true);
    reject([](auto& s){s.objects[5u].bytes[0u]^=::std::byte{1u};},false);
    reject([](auto& s){++s.objects[5u].values[0u].low_bits;},false);
    reject([](auto& s){++s.objects[5u].values[1u].low_bits;},false);
    reject([](auto& s){++s.objects[5u].words[6u];},false);
    reject([](auto& s){++s.objects[5u].words[7u];},false);
    reject([](auto& s){s.objects[6u].bytes[0u]^=::std::byte{1u};},false); // future event after replay cursor
    reject([](auto& s){++s.replay_event_cursor;},false);
    reject([](auto& s){s.recording_id[1u]=::std::byte{1u};},false);
    auto malformed{state}; malformed.objects[2u].words[3u]=UINTPTR_MAX;
    REQUIRE(id::derive(malformed).observation==id::status::invalid_graph); ++negatives;
    malformed=state; cp::value pointer{}; pointer.type.kind=cp::value_kind::reference; pointer.type.heap=cp::heap_kind::func;
    pointer.type.nullable=true; pointer.reference=cp::reference_kind::function; pointer.target=UINTPTR_MAX; malformed.retained_roots={pointer};
    REQUIRE(id::derive(malformed).observation==id::status::invalid_graph); ++negatives;
    malformed=state; malformed.objects[2u].bytes.clear(); REQUIRE(id::derive(malformed).observation==id::status::inconsistent_function); ++negatives;
    auto duplicate{state}; duplicate.objects.push_back(state.objects[2u]);
    REQUIRE(id::derive(duplicate).functions==derived.functions); // exact same-instance code alias
    ++duplicate.objects.back().words[1u]; REQUIRE(id::derive(duplicate).observation==id::status::inconsistent_function); ++negatives;
    auto multi{state}; cp::object instance{}; instance.kind=cp::object_kind::instance; instance.words[0u]=1u; instance.links={1u,9u};
    multi.objects.push_back(instance); auto function{state.objects[2u]}; function.links={8u}; ++function.words[1u]; multi.objects.push_back(function);
    multi.root_instances.push_back(8u); REQUIRE(cp::validate_graph(multi)==cp::error::none);
    auto const multiple{id::derive(multi)}; REQUIRE(multiple.observation==id::status::derived_data && multiple.functions!=derived.functions);
    cp::limits quota{}; quota.max_objects=state.objects.size()-1u;
    REQUIRE(id::derive(state,quota).observation==id::status::limit_exceeded); ++negatives;
    REQUIRE(id::derive(state,{},bind::limits{0u,0u}).observation==id::status::limit_exceeded); ++negatives;
    auto old{saved}; old.revision=1u; REQUIRE(bind::validate_shape(old)==bind::status::unknown_identity_revision); ++negatives;
    // Even fully self-consistent supplied build/environment fields remain DATA.
    // No nonce, hash, scalar pointer token or codec result constructs the private
    // runtime capture, execution retirement or manager transaction authority.
    REQUIRE(!old.grants_restore_authority());
    ::fast_io::io::println("CHECKPOINT_STATE_IDENTITY negatives=",::fast_io::mnp::dec(negatives),
        " multi_instance=1 future_event=1 canonical_endian=1 restore_authority=0");
}

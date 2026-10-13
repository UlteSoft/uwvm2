#include <uwvm2/uwvm/debugger/checkpoint_database.h>
#include <fast_io_dsal/string_view.h>
#include <algorithm>
#include <array>
#include <limits>

// Detached continuation DATA/codec regression, never a VM/restore issuer.
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
static void require(bool valid, char const* message)
{
    if(valid) { return; }
    ::fast_io::print(::fast_io::err(), "continuation ownership FAIL: ", ::fast_io::mnp::os_c_str(message), "\n");
    ::fast_io::fast_terminate();
}
static cp::state sample()
{
    cp::state s{}; s.recording_id[0u] = ::std::byte{1u}; s.checkpoint_id = 1u; s.next_logical_thread = 2u;
    s.objects.resize(12u); s.root_instances = {2u};
    auto& module{s.objects[0u]}; module.kind = cp::object_kind::module;
    module.bytes = {::std::byte{}, ::std::byte{'a'}, ::std::byte{'s'}, ::std::byte{'m'}, ::std::byte{1u}, ::std::byte{}, ::std::byte{}, ::std::byte{}};
    auto& instance{s.objects[1u]}; instance.kind = cp::object_kind::instance;
    instance.words[0u] = 1u; instance.words[2u] = 1u; instance.words[4u] = 1u; instance.links = {1u,3u,4u,5u};
    auto& function{s.objects[2u]}; function.kind = cp::object_kind::function; function.words[1u] = 1u; function.links = {2u};
    auto& memory{s.objects[3u]}; memory.kind = cp::object_kind::memory; memory.flags = 1u; memory.words = {32u,1u,1u,1u};
    auto& tag{s.objects[4u]}; tag.kind = cp::object_kind::tag; tag.links = {1u};
    auto& thread{s.objects[5u]}; thread.kind = cp::object_kind::thread; thread.words[0u] = 1u; thread.links = {7u,8u};
    for(::std::size_t n{}; n != 2u; ++n)
    {
        auto& frame{s.objects[6u+n]}; frame.kind = cp::object_kind::frame;
        frame.links = {3u,9u+n,11u+n}; frame.words = {1u,0u,0u,1u,1u,1u};
        auto& control{s.objects[8u+n]}; control.kind = cp::object_kind::control; control.flags = 3u; control.words[1u] = 2u;
        auto& handler{s.objects[10u+n]}; handler.kind = cp::object_kind::handler; handler.links = {5u}; handler.words[1u] = 2u;
    }
    return s;
}
static cp::object wait()
{
    cp::object w{}; w.kind = cp::object_kind::atomic_wait; w.links = {4u}; w.words[1u] = 4u; return w;
}
static cp::state waiting()
{
    auto s{sample()}; s.objects[5u].flags = 2u; s.objects[5u].links.push_back(13u); s.objects.push_back(wait()); return s;
}
static cp::state negative(unsigned n)
{
    auto s{sample()};
    switch(n)
    {
        case 0u: s.objects[7u].links[1u] = 9u; break; // Two frames share one control; control 10 is orphaned.
        case 1u: s.objects[7u].links[2u] = 11u; break; // Two frames share one handler.
        case 2u: s.objects[6u].words[3u] = 2u; s.objects[6u].links = {3u,9u,9u,11u}; break;
        case 3u: s.objects[6u].words[4u] = 2u; s.objects[6u].links = {3u,9u,11u,11u}; break;
        case 4u: s.objects.push_back(s.objects[8u]); break; // Orphan control.
        case 5u: s.objects.push_back(s.objects[10u]); break; // Orphan handler.
        case 6u: s.objects[10u].words[0u] = 1u; break; // Handler depth is outside its own frame.
        case 7u: s.objects[6u].words[3u] = 0u; s.objects[6u].links = {3u,11u}; break;
        case 8u: s = waiting(); s.objects[5u].flags = 0u; break;
        case 9u: s = waiting(); s.objects[5u].flags = 1u; break;
        case 10u: s.objects[5u].flags = 3u; break; // Terminated thread still has live frames.
        case 11u: s = waiting(); s.objects[5u].flags = 3u; break;
        case 12u: s = waiting(); s.objects.push_back(wait()); s.objects[5u].links.push_back(14u); break;
        case 13u: s = waiting(); s.objects[5u].links = {7u,13u,8u}; break;
        case 14u: s.objects[5u].flags = 2u; break; // Waiting state without its actual wait record.
        case 15u: s.objects.resize(6u); s.objects[5u].flags = 2u; s.objects[5u].links = {7u}; s.objects.push_back(wait()); break;
        default: require(false,"negative case index");
    }
    return s;
}
static ::std::vector<::std::byte> encode(cp::state const& s)
{
    ::std::vector<unsigned char> storage(65536u);
    ::fast_io::basic_obuffer_view<unsigned char> out{storage.data(),storage.data()+storage.size()};
    require(cp::encode_database(out,s) == cp::error::none,"positive encode");
    auto const size{static_cast<::std::size_t>(out.curr_ptr-storage.data())}; ::std::vector<::std::byte> bytes(size);
    for(::std::size_t i{}; i != size; ++i) { bytes[i] = static_cast<::std::byte>(storage[i]); } return bytes;
}
static void positive(cp::state const& s)
{
    require(cp::validate_graph(s) == cp::error::none,"legal graph ownership/aliasing");
    auto const bytes{encode(s)}; cp::state output{};
    require(cp::decode_database(bytes,output) == cp::error::none && output == s && encode(output) == bytes,"canonical positive roundtrip");
}
int main(int argc, char** argv)
{
    require(argc == 3,"private test directory and exact mode");
    auto const mode{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[2])}};
    bool const baseline{mode == "--baseline"}; require(baseline || mode == "--fixed","exact test mode");
    static_assert(cp::database_format_version == 6u);
    unsigned positives{};
    for(unsigned flags{}; flags != 2u; ++flags)
    { auto s{sample()}; s.objects[5u].flags = flags; positive(s); ++positives; }
    positive(waiting()); ++positives;
    {
        auto s{sample()}; s.objects.resize(6u); s.objects[5u].flags = 3u; s.objects[5u].links.clear(); positive(s); ++positives;
    }
    {
        auto s{sample()}; s.next_logical_thread = 3u; s.objects[5u].links = {7u};
        auto t{s.objects[5u]}; t.words[0u] = 2u; t.links = {8u}; s.objects.push_back(t); positive(s); ++positives;
        // Distinct waits and controls, shared memory/tag/function identities are legal.
        s.objects[5u].flags = s.objects[12u].flags = 2u; s.objects[5u].links.push_back(14u); s.objects[12u].links.push_back(15u);
        s.objects.push_back(wait()); s.objects.push_back(wait()); positive(s); ++positives;
    }
    {
        auto s{sample()}; auto const count{s.objects.size()}; ::std::reverse(s.objects.begin(),s.objects.end());
        for(auto& object:s.objects) { for(auto& id:object.links) { id = count+1u-id; } }
        for(auto& id:s.root_instances) { id = count+1u-id; } positive(s); ++positives;
    }
    {
        auto s{sample()}; s.next_logical_thread = (::std::numeric_limits<::std::uint64_t>::max)();
        s.objects[5u].words[0u] = s.next_logical_thread-1u; positive(s); ++positives;
    }
    for(unsigned n{}; n != 16u; ++n)
    {
        auto const s{negative(n)}; auto const path{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(argv[1]),"/case-",::fast_io::mnp::dec(n),".dmp")};
        if(baseline)
        {
            require(cp::validate_graph(s) == cp::error::none,"old validator must actually accept each missing restriction");
            ::fast_io::obuf_file file{path,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl,static_cast<::fast_io::perms>(0600u)};
            auto const written{cp::write_unpublished_database(file,s)};
            require(written.status == cp::error::none && written.synchronized == cp::sync_guarantee::os_file_sync,"actual old canonical file and OS sync"); file.close();
        }
        else
        {
            require(cp::validate_graph(s) == cp::error::invalid_shape,"fixed ownership and waiting shape refusal");
            ::std::array<unsigned char,65536u> storage{};
            ::fast_io::basic_obuffer_view<unsigned char> out{storage.data(),storage.data()+storage.size()};
            require(cp::encode_database(out,s) == cp::error::invalid_shape && out.curr_ptr == storage.data(),"invalid graph emits no header or partial output");
            // Baseline produced these real state6 files with complete SHA256 footers.
            // This stable standalone test file is DATA, never a sealed VM asset.
            ::fast_io::native_file_loader input{path,::fast_io::open_mode::in}; auto output{sample()}; auto const before{output};
            require(cp::decode_database(input,output) == cp::error::invalid_shape && output == before,"correctly hashed malformed file refused without replacing output");
        }
    }
    if(!baseline)
    {
        auto s{sample()}; s.objects[5u].links.push_back(7u);
        require(cp::validate_graph(s) == cp::error::invalid_shape,"existing duplicate frame refusal retained");
        s = waiting(); s.objects[5u].links.push_back(13u);
        require(cp::validate_graph(s) == cp::error::invalid_shape,"existing duplicate wait refusal retained");
        s = sample(); s.objects[6u].links[1u] = 11u;
        require(cp::validate_graph(s) == cp::error::invalid_reference,"wrong continuation kind retained");
    }
    ::fast_io::print("continuation-ownership ",::fast_io::mnp::os_c_str(baseline ? "BASELINE reproduced" : "FIXED PASS")," positives=",::fast_io::mnp::dec(positives),
        " newly-rejected=16 state-schema=6 whole-instance-restore=0\n");
}

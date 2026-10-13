// DATA-only quotas/grammar/retirement. These labels DO NOT qualify native
// capture, read permission, GC roots or VM restore; runtime fixture is separate.
#include <uwvm2/uwvm/debugger/wasm_path.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
#include <bit>
#include <string_view>
namespace wp=::uwvm2::uwvm::debugger::wasm_path;
namespace ws=::uwvm2::uwvm::debugger::wasm_state;
namespace dbg=::uwvm2::uwvm::debugger;
static void require(bool valid,::fast_io::string_view text)
{ if(!valid) { ::fast_io::println(::fast_io::err(),"path DATA test: ",text);::fast_io::fast_terminate(); } }
int main(int argc,char const** argv)
{
    if(argc==2 && ::std::string_view{argv[1]}=="--require-big")
    { if(::std::endian::native!=::std::endian::big) { return 90; } }
    else if(argc!=1) { return 91; }
    auto create{dbg::parse_console_command("path create globals 7 3 0 0 9 1 4")};
    require(create.kind==dbg::console_command_kind::wasm_path && wp::valid(create.wasm_path_request) &&
        create.wasm_path_request.suffix_size==2u && create.wasm_path_request.root.first==9u,"original locus/suffix grammar");
    for(auto const invalid:{"path create globals 7 3 1 0 9","path create globals 0 3 0 0 9","path create native 7 3 0 0 9",
        "path extend 0 1 0","path extend 1 1","path members 1 1 0 65","path members 1 1 -1 1","path clear extra",
        "path extend 1 1 18446744073709551616","path extend 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(invalid)}).kind==dbg::console_command_kind::invalid,"invalid DATA input refused"); }
    wp::stop_key key{};key.stop=41u;key.runtime_epoch=11u;key.cohort={{7u,3u,0u,4u,11u,2u},{8u,3u,1u,5u,11u,1u}};
    require(wp::valid(key),"finite sorted complete key DATA");
    wp::ledger first{},second{};require(first.session()!=0u && second.session()!=first.session(),"unique session labels");
    auto prepared{first.prepare(create.wasm_path_request,key)};require(prepared.data_prepared,"prepare original selectors only");
    auto published{first.commit(prepared,key)};require(published.result==ws::status::available && published.depth==2u,"DATA commit");
    wp::request page{};page.action=wp::operation::members;page.session=published.session;page.handle=published.handle;page.count=64u;
    require(first.prepare(page,key).data_prepared && !second.prepare(page,key).data_prepared,"cross-session DATA replay refused");
    auto extracted{first.prepare_value(published.session,published.handle,{},key)};
    require(extracted.data_prepared&&extracted.query.long_path==prepared.query.long_path&&
        extracted.query.selected==ws::selection::globals&&extracted.query.module==3u&&extracted.query.first==9u&&
        extracted.proposed_handle==0u&&extracted.predecessor==0u,"value extraction preserves original selectors and issues no label");
    ::std::array<::std::uint64_t,1u> suffix{UINT64_C(4294967301)};
    auto suffix_value{first.prepare_value(published.session,published.handle,suffix,key)};
    require(suffix_value.data_prepared&&suffix_value.query.long_path.size()==3u&&
        suffix_value.query.long_path.back()==suffix.front(),"bounded appended original u64 index");
    require(!second.prepare_value(published.session,published.handle,{},key).data_prepared&&
        !first.prepare_value(published.session+1u,published.handle,{},key).data_prepared&&
        !first.prepare_value(published.session,published.handle+1u,{},key).data_prepared,
        "wrong-session and never-issued labels cannot extract selectors");
    ::std::array<::std::uint64_t,17u> oversized{};
    require(!first.prepare_value(published.session,published.handle,oversized,key).data_prepared,"suffix17 rejected BEFORE growth");
    require(first.prepare(page,key).data_prepared,"DATA extraction cannot retire original handle");
    auto changed{key};changed.cohort[1u].function_generation=2u;
    require(!first.prepare(page,changed).data_prepared,"ANY cohort generation retires path DATA");
    changed=key;changed.cohort[0u].offset+=1u;require(!first.prepare(page,changed).data_prepared,"cohort locus mismatch refused");
    require(!first.prepare_value(published.session,published.handle,{},changed).data_prepared,"changed current offset refuses value extraction");
    changed=key;changed.stop+=1u;require(!first.prepare_value(published.session,published.handle,{},changed).data_prepared,"new stop refuses value extraction");
    changed=key;changed.cohort.back().function_generation+=1u;
    require(!first.prepare_value(published.session,published.handle,{},changed).data_prepared,"replacement of ANY cohort owner refuses value extraction");
    wp::request extend{};extend.action=wp::operation::extend;extend.session=published.session;extend.handle=published.handle;
    extend.suffix_size=16u;
    auto next{first.prepare(extend,key)};require(next.data_prepared && next.query.long_path.size()==18u,"owned path beyond old16 edges");
    auto replacement{first.commit(next,key)};require(replacement.result==ws::status::available && replacement.handle!=published.handle,
        "extension returns NEW immutable path label");
    require(!first.prepare(page,key).data_prepared,"retired predecessor never aliases extension");
    require(!first.prepare_value(published.session,published.handle,{},key).data_prepared,"extended predecessor cannot be used as a value source");
    first.clear();page.handle=replacement.handle;require(!first.prepare(page,key).data_prepared,"clear retires all owned path DATA");
    auto fresh{first.commit(first.prepare(create.wasm_path_request,key),key)};
    require(fresh.result==ws::status::available && fresh.handle>replacement.handle,"clear never resets label counter");
    wp::ledger quota{};wp::request root{create.wasm_path_request};root.suffix_size=0u;root.suffix={};
    for(::std::size_t entry{};entry!=8u;++entry)
    {
        auto leaf{quota.commit(quota.prepare(root,key),key)};require(leaf.result==ws::status::available,"bounded DATA root");
        for(::std::size_t depth{};depth!=4096u;depth+=16u)
        { extend.session=leaf.session;extend.handle=leaf.handle;auto item{quota.prepare(extend,key)};
          require(item.data_prepared,"up to exact4096 original indices");leaf=quota.commit(item,key);require(leaf.result==ws::status::available,"bounded DATA extend"); }
        auto exact{quota.prepare_value(leaf.session,leaf.handle,{},key)};
        require(exact.data_prepared&&exact.query.long_path.size()==4096u,"exact4096 value extraction remains bounded");
        require(!quota.prepare_value(leaf.session,leaf.handle,suffix,key).data_prepared,"4096plus1 refuses BEFORE reserve and has no label growth");
        auto repeated{quota.prepare_value(leaf.session,leaf.handle,{},key)};
        require(repeated.data_prepared&&repeated.query.long_path==exact.query.long_path,
            "refused append leaves original immutable path intact");
        extend.handle=leaf.handle;auto overflow{quota.prepare(extend,key)};
        require(!overflow.data_prepared&&overflow.result==ws::status::resource_limit,
            "depth quota is not a stale stop; failure precedes mutation");
        page.session=leaf.session;page.handle=leaf.handle;
        require(quota.prepare(page,key).data_prepared,"depth rejection preserves the current handle");
    }
    auto ninth{quota.prepare(root,key)};require(ninth.data_prepared,"zero-index root still fits aggregate budget");
    auto extra{quota.commit(ninth,key)};extend.session=extra.session;extend.handle=extra.handle;
    auto aggregate_overflow{quota.prepare(extend,key)};
    require(!aggregate_overflow.data_prepared&&aggregate_overflow.result==ws::status::resource_limit,
        "aggregate256KiB quota is not a stale stop");
    page.session=extra.session;page.handle=extra.handle;
    require(quota.prepare(page,key).data_prepared,"aggregate rejection preserves zero-depth root");
    wp::ledger labels{};wp::reply last{};
    for(::std::size_t i{};i!=wp::maximum_entries;++i)
    { last=labels.commit(labels.prepare(root,key),key);require(last.result==ws::status::available,"128 labels fit exactly"); }
    auto label_overflow{labels.prepare(root,key)};
    require(!label_overflow.data_prepared&&label_overflow.result==ws::status::resource_limit,"129th label has explicit quota diagnostic");
    page.session=last.session;page.handle=last.handle;
    require(labels.prepare(page,key).data_prepared,"label rejection preserves existing entries");
    auto wrong_session{page};++wrong_session.session;
    require(labels.prepare(wrong_session,key).result==ws::status::stale_stop_or_generation,"wrong session remains stale");
    require(labels.prepare(page,changed).result==ws::status::stale_stop_or_generation,"changed cohort remains stale");
    labels.clear();require(labels.prepare(page,key).result==ws::status::stale_stop_or_generation,"cleared handle remains stale");
    auto resumed{labels.commit(labels.prepare(root,key),key)};
    require(resumed.result==ws::status::available&&resumed.handle>last.handle,"clear releases quota without reusing labels");
    auto inconsistent{key};inconsistent.cohort[1u].participant=7u;require(!wp::valid(inconsistent),"duplicate cohort index rejected");
    auto req{prepared.query};req.long_path.resize(4097u);require(!ws::valid(req),"native DTO validates full path before traversal");
    req=prepared.query;req.path_size=1u;require(!ws::valid(req),"fixed and compressed paths cannot mix");
    ::fast_io::println("DEEP_GC_PATH_DATA bounds4096/session256KiB/retirement PASS; native authority=false");
}

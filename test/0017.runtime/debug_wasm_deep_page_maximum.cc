// Conservative detached DATA formatter maximum only; NOT runtime GC authority.
#include <uwvm2/uwvm/debugger/wasm_path.h>
#include <fast_io.h>
#include <string_view>
namespace ws=::uwvm2::uwvm::debugger::wasm_state;
static void require(bool valid,::fast_io::string_view name)
{ if(!valid) { ::fast_io::println(::fast_io::err(),"deep page DATA maximum: ",name);::fast_io::fast_terminate(); } }
int main()
{
    auto const maximum{(::std::numeric_limits<::std::uint64_t>::max)()};
    ws::view result{};result.result=ws::status::available;result.module=result.runtime_epoch=maximum;
    result.requested={ws::selection::table,maximum,maximum,0u,maximum,maximum-1u,1u};
    result.requested.member_first=maximum-64u;result.requested.member_count=64u;
    result.requested.path_session=result.requested.path_handle=maximum;result.requested.long_path.assign(4096u,maximum);
    result.first=maximum-1u;result.total_values=maximum;result.selected_object=2u;result.graph_truncated=true;
    ws::value root{};root.type={ws::value_kind::reference,4294967295ll,true,true,maximum};
    root.available=true;root.ref.kind=ws::reference_kind::structure;root.ref.object=1u;result.rows={{maximum-1u,root}};
    result.objects.reserve(66u);
    for(::std::size_t i{};i!=66u;++i)
    {
        ws::object item{};item.identifier=i+1u;item.module=maximum;item.type_index=4294967295u;
        item.kind=i==0u ? ws::object_kind::structure : ws::object_kind::exception;
        item.tag_identity_available=true;item.tag_module=item.tag_index=maximum;
        item.total_members=maximum;item.has_more_members=item.members_truncated=true;
        if(i==1u)
        {
            item.first_member=maximum-64u;item.next_member=maximum;item.has_more_members=false;
            item.members.reserve(64u);
            for(::std::uint64_t j{};j!=64u;++j)
            {
                // first=U64MAX-64 + j<64 cannot overflow; pure original-index DATA.
                ws::value value{};value.type={ws::value_kind::reference,4294967295ll,true,true,maximum};
                value.available=true;value.ref.kind=ws::reference_kind::function;value.ref.function_identity_available=true;
                value.ref.function_module=value.ref.function_index=maximum;
                item.members.push_back({item.first_member+j,value,false,true});
            }
        }
        result.objects.push_back(::std::move(item));
    }
    require(ws::valid(result),"complete66-object conservative maximum DATA");
    auto const text{ws::format(result)};auto const all{::fast_io::concat_std("wasm-stop ",::fast_io::mnp::dec(maximum),"\n",
        "Wasm path session=",::fast_io::mnp::dec(maximum)," handle=",::fast_io::mnp::dec(maximum),
        " depth=4096 view=3 protocol=1\n",text)};
    require(all.size()<ws::maximum_reply_bytes-256u,"complete prefix and longest64 function values/64 metadata fit cap");
    require(::std::string_view{text}.find("object #66 exception tag-module=18446744073709551615")!=::std::string_view::npos &&
        ::std::string_view{text}.find("Wasm state truncated: rows=1 objects=66")!=::std::string_view::npos &&
        ::std::string_view{text}.find("path=@18446744073709551615:18446744073709551615 depth=4096")!=::std::string_view::npos,
        "last metadata/selected page survive; path full4096 indices not printed/truncated");
    result.selected_object=3u;require(!ws::valid(result),"compressed terminal must be actual retained1or2");
    result.selected_object=2u;result.rows[0u].data.ref.object=2u;require(!ws::valid(result),"original copied root identity remains1");
    ::fast_io::println("DEEP_GC_PAGE_DATA max66 fullbytes=",::fast_io::mnp::dec(all.size())," native authority=false");
}

// Owned copied DATA presentation tests; no runtime memory or stop authority.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <uwvm2/uwvm/debugger/console_messages.h>
#include <cstdio>
#include <cstdlib>
#include <string>
namespace d=::uwvm2::uwvm::debugger::source_dwarf;
namespace p=::uwvm2::uwvm::debugger::source_pretty_printer;
namespace c=::uwvm2::uwvm::debugger;
namespace {
void require(bool v,char const* what) { if(!v){std::fprintf(stderr,"text display: %s\n",what);std::exit(1);} }
unsigned digit(char v) { if(v>='0'&&v<='9')return v-'0';if(v>='a'&&v<='f')return v-'a'+10;if(v>='A'&&v<='F')return v-'A'+10;std::abort(); }
std::string decode(std::string_view escaped) {
 std::string out;
 for(std::size_t i{};i<escaped.size();++i) {
  unsigned char v=escaped[i];require(v>=32&&v<127,"all display bytes are inert ASCII");
  if(v!='\\'){out+=static_cast<char>(v);continue;}
  require(++i<escaped.size(),"escape has payload");v=escaped[i];
  if(v=='\\'||v=='"'){out+=static_cast<char>(v);continue;}
  require(v=='x'&&i+2<escaped.size(),"finite byte escape syntax");
  out+=static_cast<char>((digit(escaped[i+1])<<4u)|digit(escaped[i+2]));i+=2;
 }
 return out;
}
d::type_record scalar(char const* name,unsigned width,unsigned encoding) {
 d::type_record t{};t.name=name;t.kind=d::type_kind::scalar;t.language=0x21;t.byte_size=width;t.byte_count=width;t.encoding=encoding;t.size_known=true;return t;
}
d::member_record field(char const* name,unsigned type,unsigned at) {
 d::member_record m{};m.name=name;m.type=type;m.byte_offset=at;m.offset_known=true;return m;
}
void put(std::vector<std::byte>& a,unsigned at,unsigned width,std::uint64_t v) {
 require(at<=a.size()&&width<=a.size()-at&&width<=8,"fixture write range");
 for(unsigned i{};i<width;++i)a[at+i]=static_cast<std::byte>((v>>(8*i))&255u);
}
void string_view(unsigned width,std::string const& payload) {
 std::vector<d::type_record> ts{scalar("char",1,6),scalar("size_t",width,7)};
 d::type_record ptr{};ptr.kind=d::type_kind::pointer;ptr.byte_size=ptr.byte_count=width;ptr.size_known=true;ptr.referenced_type=0;ptr.language=0x21;ts.push_back(ptr);
 d::type_record root{};root.name="std::basic_string_view<char>";root.kind=d::type_kind::structure;root.language=0x21;root.byte_size=width*2;root.size_known=true;root.members={field("_M_str",2,0),field("_M_len",1,width)};root.template_types={0};ts.push_back(root);
 std::vector<std::byte> object(width*2),known(object.size(),std::byte{255}),memory(1024);
 put(object,0,width,64);put(object,width,width,payload.size());
 for(std::size_t i{};i<payload.size();++i)memory[64+i]=static_cast<std::byte>(static_cast<unsigned char>(payload[i]));
 std::vector<d::object_node> tree;require(d::query_object_value_with_known_bits(ts,3,object,known,tree)==d::inline_query_error::none,"build owned raw object");
 unsigned reads{};auto read=[&](std::uint64_t at,std::size_t count,std::vector<std::byte>& out) {
  ++reads;require(at==64&&count==payload.size(),"only original selected payload copied");
  out.assign(memory.begin()+at,memory.begin()+at+count);return true;
 };
 require(p::format(ts,3,object,known,known,width,read,tree)==p::outcome::formatted&&reads==1,"actual standard string view formatter");
 auto const& node=tree.front();require(node.display_text==payload,"original copied bytes retained exactly");
 auto const begin=node.pretty_summary.find('"'),end=node.pretty_summary.rfind('"');
 require(begin!=std::string::npos&&end>begin,"summary retains literal outer quotes");
 require(decode(std::string_view{node.pretty_summary}.substr(begin+1,end-begin-1))==payload,"display escape roundtrip preserves all copied bytes");
 c::controller_reply reply{};reply.source_stop_identifier=7;reply.source_object_value_available=true;reply.source_object_type=tree;
 auto cmd=c::parse_console_command("print text");
 auto human=c::console_messages::source_values(reply,cmd,"source-value stop=7 thread=1");
 std::string text{human.data(),human.size()};
 require(text.starts_with("text = string length="+std::to_string(payload.size())+" \""),"human outer quotes not wire escaped");
 require(text.ends_with("\"\n"),"human row has exactly one structural newline");
 require(text.find('\0')==std::string::npos&&text.find('\x1b')==std::string::npos&&text.find('\x9b')==std::string::npos,"no terminal controls survive human projection");
 auto wire=fast_io::concat_std(d::object_details_of(node));require(wire.find("pretty=\"")!=std::string::npos&&wire.find("\\\"")!=std::string::npos,"historical quoted wire field remains escaped");
 d::object_node nested{};nested.display_text=payload;nested.display_text_available=true;std::string preview;
 p::details::scalar_text(nested,preview);require(preview.front()=='"'&&preview.back()=='"'&&decode(std::string_view{preview}.substr(1,preview.size()-2))==payload,"nested string previews use identical escaping");
}
}
int main() {
 std::string all;for(unsigned i{};i<256;++i)all+=static_cast<char>(i);
 auto escaped=fast_io::concat_std(d::escaped_metadata_text{all});require(decode(escaped)==all,"wire byte escaping independently roundtrips every byte");
 auto human=fast_io::concat_std(d::readable_metadata_text{all});
 for(unsigned char ch:human)require(ch>=32&&ch<127,"human display rejects C0 C1 DEL invalid UTF8 and bidi bytes");
 require(fast_io::concat_std(d::readable_metadata_text{"\"a\\b\""})=="\"a\\b\"","human printable syntax is unchanged");
 auto long_text=fast_io::concat_std(d::readable_metadata_text{std::string(5000,'a')});require(long_text.size()<4200&&long_text.ends_with("... (truncated)"),"human projection retains finite input bound");
 for(unsigned width:{4u,8u})for(auto const& s:{std::string{"native pretty printer"},std::string{"A\"\\\n\0\x1b\x9b\xe2\x80\xaeZ",11}})string_view(width,s);
 std::puts("{\"passed\":true,\"scope\":\"copied DATA; string byte semantics and CLI projection, no runtime authority qualification\"}");
}

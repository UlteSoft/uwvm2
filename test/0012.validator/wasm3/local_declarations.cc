// Core 3 compressed local declarations and definite-initialization rules.
// --module is an INIT-STATE checker for the pinned local_init.wast instruction subset;
// it deliberately does not claim complete operand-stack or full-module validation.
#include <uwvm2/validation/standard/wasm3/local_declarations.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <span>
#include <sys/mman.h>
#include <unistd.h>
namespace v3=uwvm2::validation::standard::wasm3;
namespace t3=uwvm2::parser::wasm::standard::wasm3::type;
using reader=v3::recursive_binary_details::reader;
using bytes=std::vector<std::byte>;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> data){bytes result;for(auto x:data)result.push_back(std::byte(x));return result;}
unsigned checks{};
v3::recursive_type_context context()
{
 auto data=raw({1,0x60,0,0});auto p=static_cast<std::byte const*>(data.data());t3::recursive_type_section section;
 CHECK(v3::scan_core3_type_section(p,p+data.size(),section).error==v3::recursive_type_binary_error::ok);
 v3::recursive_type_context result;CHECK(v3::validate_core3_type_section(section,result).error==v3::recursive_type_validation_error::ok);return result;
}
v3::core3_local_declarations decode(bytes const& data,bool expected,std::uint64_t params=0)
{
 auto ctx=context();auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(data.size()<page);
 auto base=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(base!=MAP_FAILED);
 CHECK(mprotect(base+page,page,PROT_NONE)==0);auto end=base+page;auto begin=end-data.size();if(!data.empty())std::memcpy(begin,data.data(),data.size());
 auto cursor=static_cast<std::byte const*>(begin);v3::core3_local_declarations result;result.total_count=12345;
 auto status=v3::scan_core3_local_declarations(cursor,end,params,ctx,result);CHECK((status.error==v3::core3_local_error::ok)==expected);
 if(expected)CHECK(cursor==end);else{CHECK(cursor==begin&&result.total_count==12345&&result.runs.empty());CHECK(status.binary_offset<=data.size());}
 CHECK(munmap(base,page*2)==0);++checks;return result;
}
int check_module(bytes const& data)
{
 CHECK(data.size()>=8);reader module{{data.data(),data.size()},8};t3::recursive_type_section types;v3::recursive_type_context ctx;
 std::vector<t3::sub_type const*> definitions;std::vector<unsigned> funcs;unsigned checked_functions{};
 while(module.position<data.size())
 {
  unsigned id{};std::uint_least32_t size{};CHECK(module.byte(id)&&module.u32(size));CHECK(size<=data.size()-module.position);
  auto begin=data.data()+module.position;auto end=begin+size;reader section{{begin,size}};module.position+=size;
  if(id==1)
  {
   CHECK(v3::scan_core3_type_section(begin,end,types).error==v3::recursive_type_binary_error::ok);
   CHECK(v3::validate_core3_type_section(types,ctx).error==v3::recursive_type_validation_error::ok);
   for(auto const& group:types.groups)for(auto const& t:group.types)definitions.push_back(&t);
  }
  else if(id==3)
  {
   std::uint_least32_t count{};CHECK(section.u32(count));for(unsigned i=0;i<count;++i){std::uint_least32_t type{};CHECK(section.u32(type));funcs.push_back(type);}
   CHECK(section.position==size);
  }
  else if(id==10)
  {
   std::uint_least32_t count{};CHECK(section.u32(count)&&count==funcs.size());
   for(unsigned i=0;i<count;++i)
   {
    std::uint_least32_t body_size{};CHECK(section.u32(body_size)&&body_size<=size-section.position);
    auto body=begin+section.position;auto body_end=body+body_size;section.position+=body_size;
    CHECK(funcs[i]<definitions.size());auto const& signature=*definitions[funcs[i]];
    v3::core3_local_declarations locals;
    CHECK(v3::scan_core3_local_declarations(body,body_end,signature.parameters.size(),ctx,locals).error==v3::core3_local_error::ok);
    reader code{{body,std::size_t(body_end-body)}};v3::core3_local_initialization state;
    struct frame{std::size_t checkpoint;unsigned kind;bool else_seen;};std::vector<frame> frames{{state.checkpoint(),0,false}};
    bool finished=false;
    while(code.position<code.input.size())
    {
     unsigned opcode{};CHECK(code.byte(opcode));
     if(opcode>=0x20&&opcode<=0x22)
     {
      std::uint_least32_t index{};CHECK(code.u32(index));bool initialized;
      if(index<signature.parameters.size())initialized=true;
      else {t3::core_value_type type;CHECK(locals.find(index,type));initialized=v3::core3_value_is_defaultable(type);}
      if(opcode==0x20&&!state.is_initialized(index,initialized))
      {std::printf("uninitialized local function=%u local=%u\n",i,unsigned(index));return 1;}
      if(opcode!=0x20)state.initialize(index,initialized);
     }
     else if(opcode==2||opcode==3||opcode==4||opcode==0x1f)
     {
      CHECK(code.position<code.input.size());unsigned prefix=std::to_integer<unsigned>(code.input[code.position]);
      if(prefix==0x40){unsigned ignored;CHECK(code.byte(ignored));}
      else if(prefix==0x63||prefix==0x64||(prefix>=0x69&&prefix<=0x7f)){t3::core_value_type type;CHECK(code.value(type));}
      else {std::int_least64_t index;CHECK(code.s33(index)&&index>=0&&std::uint64_t(index)<definitions.size());}
      // try_table catch vectors require the exception fixture; never silently classify an unparsed immediate.
      if(opcode==0x1f)return 77;
      frames.push_back({state.checkpoint(),opcode,false});
     }
     else if(opcode==5){CHECK(!frames.empty()&&frames.back().kind==4&&!frames.back().else_seen);CHECK(state.restore(frames.back().checkpoint));frames.back().else_seen=true;}
     else if(opcode==11){CHECK(!frames.empty());CHECK(state.restore(frames.back().checkpoint));frames.pop_back();if(frames.empty()){CHECK(code.position==code.input.size());finished=true;break;}}
     else if(opcode==0x41){std::int_least64_t ignored;CHECK(code.s33(ignored));}
     else if(opcode==0||opcode==1||opcode==0x1a){}
     else {std::fprintf(stderr,"unsupported fixture opcode %x\n",opcode);return 77;}
    }
    CHECK(finished);++checked_functions;
   }
   CHECK(section.position==size);
  }
  else if(id!=0&&id!=7){std::fprintf(stderr,"unsupported fixture section %u\n",id);return 77;}
 }
 CHECK(checked_functions==funcs.size());std::printf("initialized locals: functions=%u\n",checked_functions);return 0;
}
int main(int argc,char**argv)
{
 if(argc==2){std::ifstream f(argv[1],std::ios::binary);CHECK(f.good());std::vector<char> chars((std::istreambuf_iterator<char>(f)),{});bytes data(chars.size());std::memcpy(data.data(),chars.data(),chars.size());return check_module(data);}
 for(auto valid:{raw({0}),raw({1,1,0x64,0x6f}),raw({2,2,0x7f,1,0x63,0}),raw({1,0,0x64,0x70})})
 {decode(valid,true);for(std::size_t n=0;n<valid.size();++n)decode(bytes(valid.begin(),valid.begin()+n),false);}
 decode(raw({1,0,0x63,1}),false);decode(raw({1,1,0x63,1}),false);
 auto huge=decode(raw({1,0xff,0xff,0xff,0xff,0x0f,0x64,0x6f}),true);CHECK(huge.total_count==0xffffffffull&&huge.runs.size()==1);
 t3::core_value_type type;CHECK(huge.find(0xfffffffeull,type)&&!v3::core3_value_is_defaultable(type));CHECK(!huge.find(0xffffffffull,type));
 decode(raw({1,0xff,0xff,0xff,0xff,0x0f,0x7f}),false,1);
 decode(raw({2,0xff,0xff,0xff,0xff,0x0f,0x7f,1,0x7f}),false);
 auto zeros=decode(raw({5,0,0x7f,2,0x7e,0,0x7f,1,0x70,0,0x7f}),true,3);
 CHECK(!zeros.find(2,type)&&zeros.find(3,type)&&type.kind==t3::value_kind::i64);
 CHECK(zeros.find(4,type)&&type.kind==t3::value_kind::i64);CHECK(zeros.find(5,type)&&type.kind==t3::value_kind::reference);CHECK(!zeros.find(6,type));
 v3::core3_local_initialization state;CHECK(!state.is_initialized(7,false)&&state.is_initialized(7,true));
 state.initialize(7,false);auto outer=state.checkpoint();state.initialize(7,false);CHECK(state.checkpoint()==outer);
 state.initialize(8,false);auto inner=state.checkpoint();state.initialize(0xfffffffeu,false);CHECK(state.restore(inner)&&!state.is_initialized(0xfffffffeu,false));
 CHECK(state.is_initialized(8,false)&&state.restore(outer)&&!state.is_initialized(8,false)&&state.is_initialized(7,false));
 state.initialize(9,true);CHECK(state.checkpoint()==outer);CHECK(!state.restore(outer+1));CHECK(state.restore(0)&&!state.is_initialized(7,false));
 std::printf("PASS Core 3 local declarations/init: %u guarded cases, compressed 2^32-1 locals, scope rollback, sparse initialization\n",checks);
}

// Every prefix ends against a protected page. Malformed input must preserve cursor and publish no clauses.
#include <uwvm2/validation/standard/wasm3/exception_immediate.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace v=uwvm2::validation::standard::wasm3;using bytes=std::vector<std::byte>;unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> values){bytes b;for(auto x:values)b.push_back(std::byte(x));return b;}
void leb(bytes& b,unsigned n,unsigned width=0){unsigned i{};do{auto x=n&127;n>>=7;++i;b.push_back(std::byte(x|((n||i<width)?128:0)));}while(n||i<width);}
auto scan(bytes const& b,bool valid,unsigned count=0){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));auto size=((b.size()+page-1)/page)*page;auto memory=static_cast<std::byte*>(mmap(nullptr,size+page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED);CHECK(mprotect(memory+size,page,PROT_NONE)==0);auto begin=memory+size-b.size();if(!b.empty())std::memcpy(begin,b.data(),b.size());auto curr=static_cast<std::byte const*>(begin);auto r=v::scan_exception_catches(curr,begin+b.size());CHECK((r.error==v::exception_immediate_error::ok)==valid);CHECK(curr==(valid?begin+b.size():begin));CHECK(valid?r.clauses.size()==count:r.clauses.empty());CHECK(munmap(memory,size+page)==0);return r;
}
int main(){
 std::byte const* nil{};auto empty=v::scan_exception_catches(nil,nil);CHECK(empty.error==v::exception_immediate_error::binary&&nil==nullptr&&empty.clauses.empty());
 for(unsigned count:{0,1,2,127,128,1024}){
  bytes b;leb(b,count);for(unsigned i=0;i<count;++i){unsigned kind=i%4;b.push_back(std::byte(kind));if(kind<2)leb(b,i);leb(b,0xffffffffu-i);}
  auto r=scan(b,true,count);for(unsigned i=0;i<count;++i){CHECK(unsigned(r.clauses[i].kind)==i%4);CHECK(r.clauses[i].label_index==0xffffffffu-i);if(i%4<2)CHECK(r.clauses[i].tag_index==i);}
  if(count<3)for(std::size_t prefix=0;prefix<b.size();++prefix)scan(bytes(b.begin(),b.begin()+prefix),false);
 }
 for(unsigned kind=0;kind!=4;++kind)for(unsigned width=1;width<=5;++width){bytes b;leb(b,1,width);b.push_back(std::byte(kind));if(kind<2)leb(b,0xffffffffu);leb(b,0,width);auto r=scan(b,true,1);CHECK(r.clauses[0].label_index==0);}
 for(unsigned kind=4;kind!=256;++kind){auto r=scan(raw({1,kind,0}),false);CHECK(r.error==v::exception_immediate_error::invalid_catch_kind&&r.binary.error_offset==1);}
 for(auto const& b:{raw({}),raw({0x80}),raw({0xff,0xff,0xff,0xff,0x0f}),raw({0x80,0x80,0x80,0x80,0x10}),raw({1,0,0x80}),raw({1,0,0,0x80}),raw({1,2,0x80}),raw({1,2,0x80,0x80,0x80,0x80,0x10}),raw({1,2,0x80,0x80,0x80,0x80,0x80,0})})scan(b,false);
 // The count bounds allocation but does not consume bytes from the following instruction.
 auto b=raw({0,0x0b});auto curr=b.data();std::byte const* p=curr;auto r=v::scan_exception_catches(p,b.data()+b.size());CHECK(r.error==v::exception_immediate_error::ok&&p==b.data()+1&&r.clauses.empty());
 std::printf("PASS exception catch decoder: %u guard-page/transaction/padded-LEB checks\n",checks);
}

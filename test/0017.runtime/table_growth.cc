#define UWVM_DISABLE_INT
#define UWVM_DISABLE_JIT
#ifndef UWVM
#define UWVM 2
#endif
#include <uwvm2/uwvm/runtime/storage/impl.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
namespace storage=uwvm2::uwvm::runtime::storage;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
int main()
{
 storage::local_defined_table_storage_t table{};
 storage::local_defined_table_elem_storage_t null{},value{};int host{};
 value.type=storage::local_defined_table_elem_storage_type_t::extern_ref;value.storage.extern_ptr=&host;
 CHECK(storage::try_grow_table_elements(table,0,value));CHECK(table.elems.data()==nullptr);
 table.elems.resize(1);table.elems.index_unchecked(0)=value;
 CHECK(storage::try_grow_table_elements(table,3,null));
 CHECK(table.elems.size()==3&&table.elems.index_unchecked(0).storage.extern_ptr==&host);
 CHECK(table.elems.index_unchecked(1).storage.imported_ptr==nullptr&&table.elems.index_unchecked(2).storage.imported_ptr==nullptr);
 auto base=table.elems.data();auto capacity=table.elems.capacity();
 CHECK(storage::try_grow_table_elements(table,3,value));CHECK(table.elems.data()==base&&table.elems.capacity()==capacity);
 CHECK(!storage::try_grow_table_elements(table,2,value));
 CHECK(!storage::try_grow_table_elements(table,std::numeric_limits<std::size_t>::max(),value));
 CHECK(table.elems.size()==3&&table.elems.data()==base&&table.elems.capacity()==capacity);
 auto pid=fork();CHECK(pid>=0);
 if(pid==0)
 {
  rlimit original{};CHECK(getrlimit(RLIMIT_AS,&original)==0);rlimit restricted=original;restricted.rlim_cur=0;
  CHECK(setrlimit(RLIMIT_AS,&restricted)==0);
  // A 128 MiB reallocation cannot use existing small allocator bins and cannot create a new mapping.
  CHECK(!storage::try_grow_table_elements(table,(128u*1024u*1024u)/sizeof(value),value));
  CHECK(table.elems.size()==3&&table.elems.data()==base&&table.elems.capacity()==capacity);
  CHECK(table.elems.index_unchecked(0).storage.extern_ptr==&host);
  CHECK(storage::try_grow_table_elements(table,3,value));
  CHECK(setrlimit(RLIMIT_AS,&original)==0);
  CHECK(storage::try_grow_table_elements(table,7,value));CHECK(table.elems.index_unchecked(6).storage.extern_ptr==&host);
  std::_Exit(0);
 }
 int status{};CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);
 // Reserve followed by growth checks the branch that must reuse the existing allocation.
 table.elems.reserve(12);base=table.elems.data();CHECK(storage::try_grow_table_elements(table,9,value));CHECK(table.elems.data()==base);
 for(std::size_t i=3;i<9;++i)CHECK(table.elems.index_unchecked(i).storage.extern_ptr==&host);
 std::puts("PASS table growth: fallible native allocator, retained contents/ownership on OOM, zero growth, overflow, recovery, capacity reuse");
}

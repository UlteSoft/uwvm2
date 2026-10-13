// Successful mappings memoize indices only; epoch/address/stride misses cannot
// supply a function pointer. Live registry membership is required after a hit.
#include <uwvm2/runtime/lib/uwvm_runtime_function_identity_cache.h>
#include <cstdio>
#include <cstdlib>
using cache = uwvm2::runtime::lib::details::function_identity_cache;
static void check(bool okay, char const* message)
{ if(!okay) { std::fprintf(stderr,"function identity cache: %s\n",message);std::abort(); } }
int main()
{
    cache memo{};
    check(!memo.find(1,0x1000),"empty");
    memo.remember_validated(1,0x1000,{3,7});
    auto const index{memo.find(1,0x1000)};
    check(index && index->module==3 && index->local==7,"complete indices");
    check(!memo.find(2,0x1000),"another epoch cannot reuse the same address");
    check(!memo.find(1,0x1001),"interior/unaligned address is another identity");
    check(!memo.find(0,0x1000) && !memo.find(1,0),"invalid epoch/address");
    // 0x1000 and 0x1210 collide under the bounded index mixer.
    memo.remember_validated(1,0x1210,{9,2});
    check(!memo.find(1,0x1000) && memo.find(1,0x1210),"collision misses rather than returning another function");
    memo.remember_validated(0,0x1210,{11,12});
    check(memo.find(1,0x1210)->module==9,"invalid insert cannot overwrite a valid binding");
    memo.remember_validated(2,0x1210,{5,8});
    check(!memo.find(1,0x1210) && memo.find(2,0x1210)->local==8,"address reuse in a new epoch replaces old indices");
    memo={};check(!memo.find(2,0x1210),"execution-scope retirement clears every borrowed identity");
    check(sizeof(memo)<=1024u,"bounded metadata with no heap or code pointer");
    std::puts("PASS function identity indices: epoch/address/collision/invalid insertion/reset/1KiB bound");
}

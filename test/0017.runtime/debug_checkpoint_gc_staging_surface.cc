// Concrete PRIVATE native staging surface; no dummy issuer/authentication.
#include <type_traits>
#include <fast_io.h>
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/storage/gc_object.h>
#else
import uwvm2.uwvm.runtime.storage;
#endif
using stage=::uwvm2::uwvm::runtime::storage::checkpoint_gc_staging;
static_assert(!::std::is_default_constructible_v<stage>);
static_assert(!::std::is_copy_constructible_v<stage>);
static_assert(!::std::is_move_constructible_v<stage>);
static_assert(!::std::is_copy_assignable_v<stage>);
static_assert(!::std::is_move_assignable_v<stage>);
static_assert(::std::is_nothrow_destructible_v<stage>);
int main()
{
    ::fast_io::io::println("private GC staging surface DATA PASS; actual world issuance/native restore=false");
}

// Concurrent publication ordering test, without materializing or executing code.
// Actual concurrent native trace execution belongs to the runtime integration test.
#define UWVM_USE_LLVM_JIT
#define UWVM_DISABLE_INT
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/translate.h>
#include <array>
#include <atomic>
#include <thread>
#include <cstdio>
namespace lazy = uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
using indices_t = uwvm2::utils::container::vector<std::size_t>;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
struct context
{
    std::atomic_bool entered{};
    std::atomic_uint observers{};
    std::array<std::uintptr_t,4> recorded{};
    unsigned callbacks{};
};
void prepare(void* opaque, lazy::lazy_module_storage_t const& storage, indices_t const& indices) noexcept
{
    auto& state{*static_cast<context*>(opaque)};
    ++state.callbacks;
    for(auto const index:indices)
    {
        auto const& record{storage.materialized_functions.index_unchecked(index)};
        REQUIRE(!lazy::details::load_lazy_materialized_ready(record,std::memory_order_acquire));
        REQUIRE(record.entry_address==0x1000+index&&record.raw_entry_address==0x2000+index);
    }
    state.entered.store(true,std::memory_order_release);
    while(state.observers.load(std::memory_order_acquire)!=4) { std::this_thread::yield(); }
    // These ordinary writes occur AFTER every concurrent reader has confirmed
    // the gates are closed. Each gate's eventual acquire must expose all of
    // them, including other members reached through a direct intra-group call.
    for(auto const index:indices) { state.recorded[index]=storage.materialized_functions.index_unchecked(index).raw_entry_address; }
}
int main()
{
    for(unsigned iteration{};iteration!=64;++iteration)
    {
        lazy::lazy_module_storage_t storage;
        storage.materialized_functions.resize(4);
        indices_t indices;
        if(iteration%2) { indices.push_back(1); }
        else { for(std::size_t index:{2,0,3}) { indices.push_back(index); } }
        for(auto const index:indices)
        {
            auto& record{storage.materialized_functions.index_unchecked(index)};
            record.entry_address=0x1000+index;
            record.raw_entry_address=0x2000+index;
        }
        context state;
        lazy::lazy_compile_options options;
        options.prepare_materialized_group=prepare;
        options.prepare_user_data=std::addressof(state);
        std::array<std::thread,4> readers;
        for(std::size_t reader{};reader!=readers.size();++reader)
        {
            readers[reader]=std::thread([&,reader]
            {
                while(!state.entered.load(std::memory_order_acquire)) { std::this_thread::yield(); }
                for(auto const index:indices)
                {
                    std::uintptr_t address{};
                    REQUIRE(!lazy::try_get_lazy_raw_entry_address(storage,index,address));
                }
                state.observers.fetch_add(1,std::memory_order_acq_rel);
                auto const observed_index{indices.index_unchecked(reader%indices.size())};
                std::uintptr_t address{};
                while(!lazy::try_get_lazy_raw_entry_address(storage,observed_index,address)) { std::this_thread::yield(); }
                REQUIRE(address==0x2000+observed_index);
                for(auto const index:indices) { REQUIRE(state.recorded[index]==0x2000+index); }
            });
        }
        lazy::details::publish_lazy_materialized_group_after_prepare(storage,options,indices);
        for(auto& reader:readers) { reader.join(); }
        REQUIRE(state.callbacks==1);
        for(std::size_t index{};index!=4;++index)
        {
            bool selected{};
            for(auto const published:indices) { selected|=published==index; }
            REQUIRE(lazy::details::load_lazy_materialized_ready(storage.materialized_functions.index_unchecked(index),std::memory_order_acquire)==selected);
        }
    }
    std::puts("PASS 64 single/group publications, 256 concurrent acquire readers, all group metadata before first ready");
}

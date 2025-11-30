#include <cstddef>
#include <cstdint>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/sched_yield.h>

namespace
{
    using wasi_errno_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t;

    static bool g_sched_yield_called{};
    static wasi_errno_t g_next_return_value{wasi_errno_t::esuccess};

    wasi_errno_t test_sched_yield_callback() noexcept
    {
        g_sched_yield_called = true;
        return g_next_return_value;
    }
}  // namespace

int main()
{
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Case 0: default implementation (no callback) should return esuccess
    {
        env.wasip1_sched_yield_func_ptr = nullptr;

        auto const err = ::uwvm2::imported::wasi::wasip1::func::sched_yield(env);
        if(err != wasi_errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sched_yield: default implementation failed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: custom callback should be invoked and its errno propagated
    {
        g_sched_yield_called = false;
        g_next_return_value = wasi_errno_t::eagain;

        env.wasip1_sched_yield_func_ptr = &test_sched_yield_callback;

        auto const err = ::uwvm2::imported::wasi::wasip1::func::sched_yield(env);

        if(!g_sched_yield_called)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sched_yield: callback not invoked");
            ::fast_io::fast_terminate();
        }

        if(err != g_next_return_value)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sched_yield: errno mismatch");
            ::fast_io::fast_terminate();
        }
    }
}

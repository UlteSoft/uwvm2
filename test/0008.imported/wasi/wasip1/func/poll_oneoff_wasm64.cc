#include <cstddef>
#include <cstdint>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/poll_oneoff_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::eventtype_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::subclockflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::userdata_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;
using ::uwvm2::imported::wasi::wasip1::func::wasi_event_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::func::wasi_subscription_wasm64_t;

int main()
{
    native_memory_t memory{};
    memory.init_by_page_count(4uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {.fd_limit = 64uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    constexpr wasi_void_ptr_wasm64_t P_SUBS{1024u};
    constexpr wasi_void_ptr_wasm64_t P_EVENTS{4096u};
    constexpr wasi_void_ptr_wasm64_t P_NEVENTS{8192u};

    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff_wasm64(env,
                                                                                   static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                   static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                   static_cast<wasi_size_wasm64_t>(0u),
                                                                                   static_cast<wasi_void_ptr_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval) { ::fast_io::fast_terminate(); }
    }

    {
        wasi_subscription_wasm64_t sub{};
        sub.userdata = static_cast<userdata_wasm64_t>(static_cast<::std::uint64_t>(0x1122334455667788ull));
        sub.u.tag = eventtype_wasm64_t::eventtype_clock;
        sub.u.u.clock.id = clockid_wasm64_t::clock_monotonic;
        sub.u.u.clock.timeout = static_cast<timestamp_wasm64_t>(static_cast<::std::uint64_t>(1'000'000ull));
        sub.u.u.clock.precision = static_cast<timestamp_wasm64_t>(static_cast<::std::uint64_t>(0u));
        sub.u.u.clock.flags = static_cast<subclockflags_wasm64_t>(0u);

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(
            memory,
            P_SUBS,
            reinterpret_cast<::std::byte const*>(::std::addressof(sub)),
            reinterpret_cast<::std::byte const*>(::std::addressof(sub)) + sizeof(sub));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff_wasm64(env,
                                                                                   P_SUBS,
                                                                                   P_EVENTS,
                                                                                   static_cast<wasi_size_wasm64_t>(1u),
                                                                                   P_NEVENTS);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

        auto const nevents = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory,
                                                                                                                                  P_NEVENTS);
        if(nevents != static_cast<wasi_size_wasm64_t>(1u)) { ::fast_io::fast_terminate(); }

        wasi_event_wasm64_t evt{};
        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm64(
            memory,
            P_EVENTS,
            reinterpret_cast<::std::byte*>(::std::addressof(evt)),
            reinterpret_cast<::std::byte*>(::std::addressof(evt)) + sizeof(evt));

        if(evt.userdata != sub.userdata || evt.error != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess ||
           evt.type != eventtype_wasm64_t::eventtype_clock)
        {
            ::fast_io::fast_terminate();
        }
    }
}

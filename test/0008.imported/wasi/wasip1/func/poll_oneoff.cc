#include <cstddef>
#include <cstdint>

#include <chrono>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/poll_oneoff.h>

using ::uwvm2::imported::wasi::wasip1::abi::clockid_t;
using ::uwvm2::imported::wasi::wasip1::abi::eventrwflags_t;
using ::uwvm2::imported::wasi::wasip1::abi::eventtype_t;
using ::uwvm2::imported::wasi::wasip1::abi::fd_t;
using ::uwvm2::imported::wasi::wasip1::abi::filesize_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::subclockflags_t;
using ::uwvm2::imported::wasi::wasip1::abi::timestamp_t;
using ::uwvm2::imported::wasi::wasip1::abi::userdata_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::imported::wasi::wasip1::func::wasi_event_t;
using ::uwvm2::imported::wasi::wasip1::func::wasi_subscription_t;
using ::uwvm2::object::memory::linear::native_memory_t;

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

    constexpr wasi_void_ptr_t P_SUBS{1024u};
    constexpr wasi_void_ptr_t P_EVENTS{4096u};
    constexpr wasi_void_ptr_t P_NEVENTS{8192u};

    // Case 0: single clock subscription should behave as sleep
    {
        wasi_subscription_t sub{};
        constexpr ::std::uint64_t timeout_ns{50'000'000ull};

        sub.userdata = static_cast<userdata_t>(static_cast<::std::uint64_t>(0x1122334455667788ull));
        sub.u.tag = eventtype_t::eventtype_clock;
        sub.u.u.clock.id = clockid_t::clock_monotonic;
        sub.u.u.clock.timeout = static_cast<timestamp_t>(timeout_ns);
        sub.u.u.clock.precision = static_cast<timestamp_t>(static_cast<::std::uint64_t>(0u));
        sub.u.u.clock.flags = static_cast<subclockflags_t>(0u);

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            P_SUBS,
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)),
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)) + sizeof(sub));

        using clock = ::std::chrono::steady_clock;
        auto const t0 = clock::now();

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(env, P_SUBS, P_EVENTS, static_cast<wasi_size_t>(1u), P_NEVENTS);
        auto const t1 = clock::now();

        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const elapsed_ns = ::std::chrono::duration_cast<::std::chrono::nanoseconds>(t1 - t0).count();
        constexpr auto min_expected_ns = static_cast<long long>(timeout_ns / 2u);

        if(elapsed_ns < min_expected_ns)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " elapsed_ns=", elapsed_ns);
            ::fast_io::fast_terminate();
        }

        auto const nevents = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, P_NEVENTS);
        if(nevents != static_cast<wasi_size_t>(1u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(nevents));
            ::fast_io::fast_terminate();
        }

        wasi_event_t evt{};
        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32(memory,
                                                                             P_EVENTS,
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)),
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)) + sizeof(evt));

        if(evt.userdata != sub.userdata || evt.error != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || evt.type != eventtype_t::eventtype_clock)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__));
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: nsubscriptions == 0 -> einval
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(env,
                                                                            static_cast<wasi_void_ptr_t>(0u),
                                                                            static_cast<wasi_void_ptr_t>(0u),
                                                                            static_cast<wasi_size_t>(0u),
                                                                            static_cast<wasi_void_ptr_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: fd_read subscription with invalid FD -> immediate ebadf event
    {
        wasi_subscription_t sub{};
        sub.userdata = static_cast<userdata_t>(static_cast<::std::uint64_t>(0xA1A2A3A4uLL));
        sub.u.tag = eventtype_t::eventtype_fd_read;
        sub.u.u.fd_readwrite.file_descriptor = static_cast<fd_t>(12345);

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            P_SUBS,
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)),
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)) + sizeof(sub));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(env, P_SUBS, P_EVENTS, static_cast<wasi_size_t>(1u), P_NEVENTS);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const nevents = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, P_NEVENTS);
        if(nevents != static_cast<wasi_size_t>(1u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(nevents));
            ::fast_io::fast_terminate();
        }

        wasi_event_t evt{};
        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32(memory,
                                                                             P_EVENTS,
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)),
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)) + sizeof(evt));

        if(evt.userdata != sub.userdata || evt.type != eventtype_t::eventtype_fd_read || evt.error != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf ||
           evt.u.fd_readwrite.nbytes != static_cast<filesize_t>(0u) || evt.u.fd_readwrite.flags != static_cast<eventrwflags_t>(0u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: fd_write subscription on FD without poll rights -> enotcapable
    {
        env.fd_storage.opens.resize(8uz);

        auto& fd = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
        fd.rights_base = static_cast<rights_t>(0);
        fd.rights_inherit = static_cast<rights_t>(0);

        wasi_subscription_t sub{};
        sub.userdata = static_cast<userdata_t>(static_cast<::std::uint64_t>(0xB1B2B3B4uLL));
        sub.u.tag = eventtype_t::eventtype_fd_write;
        sub.u.u.fd_readwrite.file_descriptor = static_cast<fd_t>(3);

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            P_SUBS,
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)),
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)) + sizeof(sub));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(env, P_SUBS, P_EVENTS, static_cast<wasi_size_t>(1u), P_NEVENTS);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const nevents = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, P_NEVENTS);
        if(nevents != static_cast<wasi_size_t>(1u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(nevents));
            ::fast_io::fast_terminate();
        }

        wasi_event_t evt{};
        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32(memory,
                                                                             P_EVENTS,
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)),
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)) + sizeof(evt));

        if(evt.userdata != sub.userdata || evt.type != eventtype_t::eventtype_fd_write ||
           evt.error != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable || evt.u.fd_readwrite.nbytes != static_cast<filesize_t>(0u) ||
           evt.u.fd_readwrite.flags != static_cast<eventrwflags_t>(0u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__));
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: fd_read subscription with invalid FD -> immediate ebadf event
    {
        wasi_subscription_t sub{};
        sub.userdata = static_cast<userdata_t>(static_cast<::std::uint64_t>(0xA1A2A3A4uLL));
        sub.u.tag = eventtype_t::eventtype_fd_read;
        sub.u.u.fd_readwrite.file_descriptor = static_cast<fd_t>(12345);

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            P_SUBS,
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)),
                                                                            reinterpret_cast<::std::byte const*>(::std::addressof(sub)) + sizeof(sub));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(env, P_SUBS, P_EVENTS, static_cast<wasi_size_t>(1u), P_NEVENTS);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const nevents = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, P_NEVENTS);
        if(nevents != static_cast<wasi_size_t>(1u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(nevents));
            ::fast_io::fast_terminate();
        }

        wasi_event_t evt{};
        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32(memory,
                                                                             P_EVENTS,
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)),
                                                                             reinterpret_cast<::std::byte*>(::std::addressof(evt)) + sizeof(evt));

        if(evt.userdata != sub.userdata || evt.type != eventtype_t::eventtype_fd_read || evt.error != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf ||
           evt.u.fd_readwrite.nbytes != static_cast<filesize_t>(0u) || evt.u.fd_readwrite.flags != static_cast<eventrwflags_t>(0u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__));
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: two fd subscriptions (invalid fd and no-rights fd) -> two events
    {
        // ensure fd 3 exists but has no rights
        env.fd_storage.opens.resize(8uz);

        auto& fd3 = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
        fd3.rights_base = static_cast<rights_t>(0);
        fd3.rights_inherit = static_cast<rights_t>(0);

        wasi_subscription_t subs[2]{};

        subs[0].userdata = static_cast<userdata_t>(static_cast<::std::uint64_t>(0xC1C2C3C4uLL));
        subs[0].u.tag = eventtype_t::eventtype_fd_read;
        subs[0].u.u.fd_readwrite.file_descriptor = static_cast<fd_t>(12345);

        subs[1].userdata = static_cast<userdata_t>(static_cast<::std::uint64_t>(0xD1D2D3D4uLL));
        subs[1].u.tag = eventtype_t::eventtype_fd_write;
        subs[1].u.u.fd_readwrite.file_descriptor = static_cast<fd_t>(3);

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            P_SUBS,
                                                                            reinterpret_cast<::std::byte const*>(subs),
                                                                            reinterpret_cast<::std::byte const*>(subs) + sizeof(subs));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(env, P_SUBS, P_EVENTS, static_cast<wasi_size_t>(2u), P_NEVENTS);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const nevents = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, P_NEVENTS);
        if(nevents != static_cast<wasi_size_t>(2u))
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(nevents));
            ::fast_io::fast_terminate();
        }

        wasi_event_t evts[2]{};
        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32(memory,
                                                                             P_EVENTS,
                                                                             reinterpret_cast<::std::byte*>(evts),
                                                                             reinterpret_cast<::std::byte*>(evts) + sizeof(evts));

        auto const evt0{evts[0]};
        auto const evt1{evts[1]};

        bool got_fd_read{};
        bool got_fd_write{};

        auto const userdata_fd_read{static_cast<userdata_t>(static_cast<::std::uint64_t>(0xC1C2C3C4uLL))};
        auto const userdata_fd_write{static_cast<userdata_t>(static_cast<::std::uint64_t>(0xD1D2D3D4uLL))};

        auto const matches = [](wasi_event_t const& evt,
                                userdata_t expected_userdata,
                                ::uwvm2::imported::wasi::wasip1::abi::eventtype_t expected_type,
                                ::uwvm2::imported::wasi::wasip1::abi::errno_t expected_error) noexcept
        {
            return evt.userdata == expected_userdata && evt.type == expected_type && evt.error == expected_error &&
                   evt.u.fd_readwrite.nbytes == static_cast<filesize_t>(0u) && evt.u.fd_readwrite.flags == static_cast<eventrwflags_t>(0u);
        };

        if(matches(evt0, userdata_fd_read, eventtype_t::eventtype_fd_read, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf) ||
           matches(evt1, userdata_fd_read, eventtype_t::eventtype_fd_read, ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf))
        {
            got_fd_read = true;
        }

        if(matches(evt0, userdata_fd_write, eventtype_t::eventtype_fd_write, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable) ||
           matches(evt1, userdata_fd_write, eventtype_t::eventtype_fd_write, ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable))
        {
            got_fd_write = true;
        }

        if(!got_fd_read || !got_fd_write)
        {
            ::fast_io::perrln(static_cast<unsigned>(__LINE__));
            ::fast_io::fast_terminate();
        }
    }
}

#pragma once

#ifndef UWVM_MODULE
// std
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view wasip1_socket_udp_connect_alias{u8"-I1udpcon"};
        inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            wasip1_socket_udp_connect_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                               ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                               ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter wasip1_socket_udp_connect{
        .name{u8"--wasip1-socket-udp-connect"},
        .describe{u8"Configure a WASI Preview 1 UDP client connection from the command line."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasip1_socket_udp_connect_alias), 1uz}},
        .handle{::std::addressof(details::wasip1_socket_udp_connect_callback)},
        .cate{::uwvm2::utils::cmdline::categorization::wasi}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

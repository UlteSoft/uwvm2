#pragma once
#ifndef UWVM_MODULE
# include <memory>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    // Repeatable; the parser prepass registers every selected module before
    // any callback can load a preload, regardless of option order.
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter secure{
        .name{u8"--secure"},
        .describe{u8"Configure offline cryptographic verification for explicitly selected WASM modules. Use --secure help for subcommands."},
        .usage{u8"[help|status|module <wasm:path> <protected-policy:path>|named-module <wasm:path> <protected-policy:path> <expected-id>]"},
        .cate{::uwvm2::utils::cmdline::categorization::secure}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

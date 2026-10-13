/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
// Parsed argument state is shared with WASI initialization. It has no
// dependency on the CLI parameter table or its callback definitions.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline
{
    // globals: There is no data contention because command line parsing is always done when the main thread is created.
    inline ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results> parsing_result{};  // [global]
    inline char8_t const* argv0_dir{};                                                                              // [global]
    inline ::uwvm2::utils::cmdline::parameter_parsing_results* wasm_file_ppos{};                                    // [global]

}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     MVP opcode aliases inherited from WebAssembly Release 1.0.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#pragma once

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::opcode::mvp
{
    using op_basic = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using constexpr_op_basic = ::uwvm2::parser::wasm::standard::wasm1::opcode::constexpr_op_basic;
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::opcode
{
    using mvp_op_basic = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::mvp::op_basic;
    using mvp_constexpr_op_basic = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::mvp::constexpr_op_basic;
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif

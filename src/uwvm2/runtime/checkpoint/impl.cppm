/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
export module uwvm2.runtime.checkpoint;
export import uwvm2.runtime.checkpoint.materialization;
export import uwvm2.runtime.checkpoint.shadow_ledger;
export import uwvm2.runtime.checkpoint.executed_initialization;
export import uwvm2.runtime.checkpoint.dynamic_native_packet;
export import uwvm2.runtime.checkpoint.caller_return_projection;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "impl.h"

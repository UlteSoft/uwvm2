/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-05
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
#  include "compact_numeric/impl.h"
#endif
# include "gc_trace_metadata.h"
# include "tag_instance_identity.h"
# include "wasm_module.h"
# include "gc_static_roots.h"
# include "storage.h"
# include "builtin_wasip1_loader.h"
# include "full.h"
# include "source_digest.h"
#endif

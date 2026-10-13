/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    // A tag is an instance identity, independent of its structural signature.
    // Keep the real definition independent of module/parser/GC storage: native
    // exception contexts retain this same type without importing a whole VM.
    // Moving the definition does not turn the identity into a GC/source root.
    // https://webassembly.github.io/spec/core/exec/runtime.html#tag-instances
    struct tag_instance_identity final {};
}

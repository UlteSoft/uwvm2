/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
// Deliberately in the global module fragment for both header and module builds.
// Only the actual Runtime translation unit supplies the real definition. The
// absent weak symbol disables caching; it never names a header/host fallback.
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
extern "C" [[gnu::weak, gnu::visibility("hidden")]] void uwvm2_runtime_cache_image_anchor() noexcept;
#endif

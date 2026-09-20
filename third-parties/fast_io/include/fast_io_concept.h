#pragma once

/*
 * Public customization vocabulary (CPO/protocol level).
 *
 * This umbrella defines the open capability plane shared by all higher-level
 * IO operations. It covers device/observer references and primitive transfer,
 * buffered cursors and whole-record status hooks, printable and scannable value
 * representations, alias/forward normalization, semantic IO nodes, and strlike
 * materialization destinations. The categories are peers in one protocol
 * vocabulary; the namespace `operations` is not by itself proof that a symbol
 * is lower or higher than every value CPO.
 *
 * A concept normally proves only an exact expression, result type, and static
 * marker. Bounds, lifetime, ownership, cursor provenance, exception behavior,
 * repeatability, and observational equivalence remain semantic obligations and
 * are documented beside the corresponding CPO. Full print/scan/concat
 * orchestration does not live here: IO-level algorithms compose these
 * capabilities after normalizing streams and source objects exactly once.
 */

#if !defined(__cplusplus)
#error "You must be using a C++ compiler"
#endif
#if !defined(__cpp_concepts)
#error "fast_io requires at least a C++20 standard compiler."
#else

#include <version>
#include <cstddef>
#include <type_traits>
#include <concepts>
#include <cstdint>
// Public protocol concepts form numeric capacity constants without relying on a higher-level umbrella header.
#include <limits>

#if defined(__HERBCEPTIONS__)
/*
`throws` changes a function's canonical type and return ABI, so recognizing the
keyword without the matching std::error definition would be an unsafe partial
configuration.  The experimental compiler publishes no versioned `__cpp_*`
macro yet; require both its language-mode macro and the runtime header rather
than inferring support from a vendor version.
*/
#if __has_include(<herbceptions/error>)
#include <herbceptions/error>
#else
#error "Herbception mode requires <herbceptions/error> from the matching runtime"
#endif
#endif

#include "fast_io_dsal/impl/misc/push_macros.h"
#include "fast_io_dsal/impl/misc/push_warnings.h"

#include "fast_io_freestanding_impl/stack/impl.h"

#include "fast_io_core_impl/freestanding/addressof.h"
#include "fast_io_core_impl/concepts/impl.h"

#include "fast_io_dsal/impl/misc/pop_macros.h"
#include "fast_io_dsal/impl/misc/pop_warnings.h"

#endif

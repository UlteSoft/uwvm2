#pragma once

/*
 * Aggregation boundary for the format frontend (FMT level).
 *
 * This file assembles the structural format vocabulary, print front doors, and
 * string-concat front doors. The parser/compiler/lowering implementation is
 * intentionally reached through those facades. Every successful operation
 * leaves this level as ordinary typed IO arguments: output goes to
 * `fast_io::io::print`, while materialization goes to the IO concat engine.
 */

// Include the standard string declaration before the fast_io string adapter.  This ordering
// makes the umbrella reliable even when a consumer has not included <string> previously.
#include <string>

#include "types.h"
#include "print.h"
#include "to.h"
#include "concat_std.h"
#include "concat_fast_io.h"

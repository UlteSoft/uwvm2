#pragma once

/*
 * Aggregation boundary for semantic print objects (CPO/semantic level).
 *
 * `pack`, `condition`, and `width` form a small typed intermediate language
 * shared by ordinary IO calls and format lowering. They describe record
 * structure and presentation without choosing an output, allocating storage,
 * selecting a printable leaf, or issuing a write. The print/concat operation
 * engines interpret and normalize this structure.
 */

#include "forward.h"
#include "traits.h"

#include "pack.h"
#include "width.h"
#include "cond.h"

#pragma once

/*
 * Aggregation boundary for string concatenation/materialization (IO level).
 *
 * Concat consumes the same normalized printable and semantic object vocabulary
 * as stream output, but replaces the device endpoint with a strlike
 * destination. The included headers provide generic destination growth and
 * the full sizing/materialization planner. Format frontends merely lower
 * syntax and forward typed components to this operation.
 */

#include "concat_buffer.h"
#include "concat_general.h"

#pragma once

/*
 * Aggregation boundary for the freestanding print operation (IO level).
 *
 * The implementation included here is the generic explicit-output engine used
 * by higher-level IO scenarios. Keeping this boundary separate from
 * `fast_io::io::print` is intentional: `operations::print_freestanding` has no
 * default-output, stderr, panic, or reporting scenario policy. It accepts an
 * output object and executes one normalized print record against it.
 */

#include "print_freestanding.h"

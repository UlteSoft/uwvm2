#pragma once

/*
 * Aggregation boundary for primitive output operations (protocol execution).
 *
 * This matrix synthesizes `write`/`pwrite` across typed characters and bytes,
 * contiguous and scatter sources, some and all completion contracts, pointer,
 * span, and iterator front doors. These operations sit below print formatting:
 * they normalize a stream observer once and move already-materialized units.
 * Device providers need supply only a coherent subset of `_define` CPOs; the
 * included algorithms derive the remaining valid forms.
 */

#include "basis.h"
#include "scatter.h"
#include "scatterbytes.h"
#include "pbasis.h"
#include "scatterp.h"
#include "scatterpbytes.h"
#include "decay.h"
#include "range.h"
#include "spanops.h"
#include "ptrops.h"

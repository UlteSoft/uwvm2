#pragma once

/*
 * Aggregation boundary for primitive input operations (protocol execution).
 *
 * This matrix synthesizes `read`/`pread` across typed characters and bytes,
 * contiguous and scatter destinations, some and all completion contracts, and
 * pointer/span front doors. These operations sit below target scanning: they
 * normalize an input observer once and fill caller-provided storage. Device
 * providers may expose a coherent subset of `_define` CPOs, from which the
 * included algorithms derive the remaining valid forms.
 */

#include "basis.h"
#include "scatter.h"
#include "scatterbytes.h"
#include "pbasis.h"
#include "scatterp.h"
#include "scatterpbytes.h"
#include "decay.h"
#include "spanops.h"
#include "ptrops.h"

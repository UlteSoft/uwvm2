#pragma once

/*
 * Master aggregation boundary for core IO operations.
 *
 * The files below compose three distinct kinds of code which intentionally
 * share the `operations` namespace family:
 *
 * 1. protocol recognition and normalization (`refs` and raw `_define` CPOs);
 * 2. primitive operations (`read`, `write`, seek, transcode, and transmit);
 * 3. complete object operations (print and scan), plus adapters used by concat
 *    and decorator/transcode filters.
 *
 * Therefore namespace membership is not a strict vertical layer. Public
 * `operations::*` functions accept raw handles or operation records;
 * `operations::defines` checks those public expressions;
 * `operations::decay` consumes observers/objects after one normalization; and
 * `operations::decay::defines` probes the exact provider capabilities used by
 * the decay algorithms. User-facing `fast_io::io::*` scenario wrappers and the
 * FMT syntax frontend are above this aggregation boundary.
 */

#include "refs/impl.h"
#include "lockguard.h"
#include "common.h"
#include "seek.h"
#include "output_operation_guard.h"
#include "multiblock_iterator_view/impl.h"

#include "writeimpl/impl.h"
#include "readimpl/impl.h"
#include "transcodeimpl/impl.h"
#include "printimpl/impl.h"
#include "scan_freestanding.h"
#include "transmitimpl/impl.h"
#include "strlike_reference_wrapper.h"
#include "decofilter.h"
#include "transcodefilter.h"

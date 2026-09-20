#pragma once

/*
 * Aggregation boundary for stream-to-stream transmit operations (IO level).
 *
 * Transmit composes normalized input and output device capabilities without
 * interpreting the payload as printable or scannable objects. The family
 * distinguishes element versus byte units, bounded some/all transfer, and
 * transfer until EOF. Provider whole-operation status CPOs may replace the
 * generic buffered read/write loop when available.
 */

#include "common.h"

#include "bytes_until_eof.h"
#include "until_eof.h"
#include "bytes_some.h"
#include "some.h"
#include "bytes_all.h"
#include "all.h"

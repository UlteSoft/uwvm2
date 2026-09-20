#pragma once

/*
 * Aggregation boundary for stream-side CPOs (protocol level).
 *
 * These headers normalize public handles to directional observers and expose
 * their seek/flush, input/output buffer, decorator, secure-clear, mutex, and
 * primitive/status capabilities. IO operations consume this vocabulary;
 * including it does not itself perform a print, scan, read, or write.
 */

#include "seek.h"
#include "base.h"
#include "input_stream.h"
#include "output_stream.h"
#include "decorators.h"
#include "secure_clear.h"
#include "mutex.h"
#include "buf.h"

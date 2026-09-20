#pragma once

/**
 * @file
 * @brief Defines the stream-adapter aggregation boundary.
 *
 * Dependency order is significant: policies and cursor bridges precede owner
 * types; directional owners precede the duplex composition; refs then expose
 * CPOs; explicit terminal operations and factories are layered last.
 */

#include "error.h"
#include "traits.h"
#include "storage.h"
#include "cursor.h"
#include "bridge.h"
#include "output.h"
#include "input.h"
#include "io.h"
#include "ref.h"
#include "finish.h"
#include "factories.h"

import { writeSync } from 'node:fs';
import { getHeapStatistics } from 'node:v8';

// Source-equivalent logical Core3 GC families. Runtime representations differ.
// Global publication remains observable after Run; no GC forcing or hot logging.
const ROOT_COUNT = 1024, WIDTH = 8, SEED = 123456789, SALT = 0xa5c31f27;
export const ROOTS = new Array(ROOT_COUNT);
globalThis.uwvmGeneralGcRoots = ROOTS;
let NODE_ROOTS;
let stepChecksum = 0, rootChecksum = 0, lastState = 0;
class Box { constructor(value) { this.value = value; } }
class Node { constructor(value, next) { this.value = value; this.next = next; } }
function next(state) { return (Math.imul(state, 1664525) + 1013904223) >>> 0; }
function pair(state) {
  const a = new Node(state, null), b = new Node((Math.imul(state, 3) + 17) >>> 0, a);
  a.next = b;
  return a;
}
function cycle(a, b) {
  if (b == null || b.next !== a) throw new Error('broken B.next=A');
}
function numericSum(cells) {
  if (cells.length !== WIDTH) throw new Error('numeric array length');
  let total = 0;
  for (let i = 0; i < WIDTH; ++i) total = (total + cells[i]) >>> 0;
  return total;
}
function referenceSum(cells, a, b) {
  if (cells.length !== WIDTH) throw new Error('reference array length');
  let total = 0;
  for (let i = 0; i < WIDTH; ++i) {
    if (cells[i] == null) throw new Error('unexpected null cell');
    total = (total + cells[i].value) >>> 0;
  }
  cycle(a, b);
  return total;
}
function finish(state, total, roots) {
  lastState = state; stepChecksum = total; rootChecksum = roots;
  return (total ^ roots) >>> 0;
}
function mutableStructAllocate(count) {
  let state = SEED, total = 0;
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const slot = i & (ROOT_COUNT - 1);
    if (i >= ROOT_COUNT) total = (total + ROOTS[slot].value) >>> 0;
    ROOTS[slot] = new Box(state);
    const a = ROOTS[slot];
    a.value = (state + i) >>> 0;
    total = (total + a.value) >>> 0;
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) roots = (roots + ROOTS[slot].value) >>> 0;
  return finish(state, total, roots);
}
function mutableStructMutate(count) {
  let state = SEED, total = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    state = next(state); ROOTS[slot] = new Box(state);
  }
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const a = ROOTS[i & (ROOT_COUNT - 1)];
    a.value = (state + i) >>> 0;
    total = (total + a.value) >>> 0;
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) roots = (roots + ROOTS[slot].value) >>> 0;
  return finish(state, total, roots);
}
function referenceCycleAllocate(count) {
  let state = SEED, total = 0;
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const slot = i & (ROOT_COUNT - 1);
    if (i >= ROOT_COUNT) {
      const olda = ROOTS[slot], oldb = olda.next;
      total = (total + olda.value + oldb.value) >>> 0;
      cycle(olda, oldb);
    }
    ROOTS[slot] = pair(state);
    const a = ROOTS[slot], b = a.next;
    a.value = (state + i) >>> 0; a.next = a;
    let observed = (a.value + a.next.value) >>> 0;
    a.next = b; observed = (observed + a.next.value) >>> 0;
    cycle(a, b); total = (total + observed) >>> 0;
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    const a = ROOTS[slot], b = a.next;
    roots = (roots + a.value + b.value) >>> 0; cycle(a, b);
  }
  return finish(state, total, roots);
}
function referenceCycleMutate(count) {
  let state = SEED, total = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) { state = next(state); ROOTS[slot] = pair(state); }
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const a = ROOTS[i & (ROOT_COUNT - 1)], b = a.next;
    a.value = (state + i) >>> 0; a.next = a;
    let observed = (a.value + a.next.value) >>> 0;
    a.next = b; observed = (observed + a.next.value) >>> 0;
    cycle(a, b); total = (total + observed) >>> 0;
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    const a = ROOTS[slot], b = a.next;
    roots = (roots + a.value + b.value) >>> 0; cycle(a, b);
  }
  return finish(state, total, roots);
}
function numericArrayAllocate(count) {
  let state = SEED, total = 0;
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const slot = i & (ROOT_COUNT - 1);
    if (i >= ROOT_COUNT) total = (total + numericSum(ROOTS[slot])) >>> 0;
    const fresh = new Uint32Array(WIDTH); fresh.fill(state); ROOTS[slot] = fresh;
    const cells = ROOTS[slot], selected = i & (WIDTH - 1), following = (selected + 1) & (WIDTH - 1);
    cells[selected] = state ^ SALT; cells[following] = (state + i) >>> 0;
    total = (total + cells[selected] + cells[following] + cells.length) >>> 0;
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) roots = (roots + numericSum(ROOTS[slot])) >>> 0;
  return finish(state, total, roots);
}
function numericArrayMutate(count) {
  let state = SEED, total = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    state = next(state);
    const fresh = new Uint32Array(WIDTH); fresh.fill(state); ROOTS[slot] = fresh;
  }
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const cells = ROOTS[i & (ROOT_COUNT - 1)], selected = i & (WIDTH - 1), following = (selected + 1) & (WIDTH - 1);
    cells[selected] = state ^ SALT; cells[following] = (state + i) >>> 0;
    total = (total + cells[selected] + cells[following] + cells.length) >>> 0;
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) roots = (roots + numericSum(ROOTS[slot])) >>> 0;
  return finish(state, total, roots);
}
function referenceArrayAllocate(count) {
  let state = SEED, total = 0;
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const slot = i & (ROOT_COUNT - 1);
    if (i >= ROOT_COUNT) {
      const olda = NODE_ROOTS[slot], oldb = olda.next;
      total = (total + referenceSum(ROOTS[slot], olda, oldb)) >>> 0;
    }
    const aFresh = pair(state), fresh = new Array(WIDTH); fresh.fill(aFresh);
    ROOTS[slot] = fresh; NODE_ROOTS[slot] = aFresh;
    const cells = ROOTS[slot], a = NODE_ROOTS[slot], b = a.next;
    a.value = (state + i) >>> 0;
    const selected = i & (WIDTH - 1), following = (selected + 1) & (WIDTH - 1);
    cells[selected] = b; cells[following] = a;
    total = (total + cells[selected].value + cells[following].value + cells.length) >>> 0;
    cycle(a, b);
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    const a = NODE_ROOTS[slot], b = a.next;
    roots = (roots + referenceSum(ROOTS[slot], a, b)) >>> 0;
  }
  return finish(state, total, roots);
}
function referenceArrayMutate(count) {
  let state = SEED, total = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    state = next(state);
    const aFresh = pair(state), fresh = new Array(WIDTH); fresh.fill(aFresh);
    ROOTS[slot] = fresh; NODE_ROOTS[slot] = aFresh;
  }
  for (let i = 0; i < count; ++i) {
    state = next(state);
    const cells = ROOTS[i & (ROOT_COUNT - 1)], a = NODE_ROOTS[i & (ROOT_COUNT - 1)], b = a.next;
    a.value = (state + i) >>> 0;
    const selected = i & (WIDTH - 1), following = (selected + 1) & (WIDTH - 1);
    cells[selected] = b; cells[following] = a;
    total = (total + cells[selected].value + cells[following].value + cells.length) >>> 0;
    cycle(a, b);
  }
  let roots = 0;
  for (let slot = 0; slot < ROOT_COUNT; ++slot) {
    const a = NODE_ROOTS[slot], b = a.next;
    roots = (roots + referenceSum(ROOTS[slot], a, b)) >>> 0;
  }
  return finish(state, total, roots);
}
const loops = Object.freeze({
  'mutable-struct/allocate': mutableStructAllocate, 'mutable-struct/mutate': mutableStructMutate,
  'reference-cycle/allocate': referenceCycleAllocate, 'reference-cycle/mutate': referenceCycleMutate,
  'numeric-array/allocate': numericArrayAllocate, 'numeric-array/mutate': numericArrayMutate,
  'reference-array/allocate': referenceArrayAllocate, 'reference-array/mutate': referenceArrayMutate
});
function run(family, phase, count) {
  const key = family + '/' + phase;
  if (!Object.hasOwn(loops, key)) throw new Error('unknown family/phase');
  return loops[key](count);
}
function u32(text) {
  if (!/^(0|[1-9][0-9]*)$/.test(text)) throw new Error('canonical u32 expected');
  const value = Number(text);
  if (!Number.isSafeInteger(value) || value < 0 || value > 0xffffffff) throw new Error('u32 range');
  return value;
}
function verify(actual, step, roots, state) {
  if (stepChecksum !== step || rootChecksum !== roots || lastState !== state || actual !== ((step ^ roots) >>> 0))
    throw new Error('independent scalar oracle/checksum mismatch');
}
const args = process.argv.slice(2), telemetry = args.length === 12 && args[11] === '--gc-telemetry';
if (args.length !== 11 && !telemetry) throw new Error('family phase count warmCount warmRounds step roots state warmStep warmRoots warmState [--gc-telemetry]');
if (!Object.hasOwn(loops, args[0] + '/' + args[1])) throw new Error('unknown family/phase');
const n = u32(args[2]), warmN = u32(args[3]), rounds = u32(args[4]);
if (n < ROOT_COUNT || n > 2000000 || warmN < ROOT_COUNT || warmN > 2000000 || rounds > 64)
  throw new Error('count/round budget exceeded');
if (args[0] === 'reference-array') {
  NODE_ROOTS = new Array(ROOT_COUNT);
  globalThis.uwvmGeneralGcNodeRoots = NODE_ROOTS;
}
const step = u32(args[5]), roots = u32(args[6]), state = u32(args[7]);
const ws = u32(args[8]), wr = u32(args[9]), wl = u32(args[10]);
for (let round = 0; round < rounds; ++round) verify(run(args[0], args[1], warmN), ws, wr, wl);
if (telemetry) writeSync(1, 'GC_TELEMETRY_START\n');
const before = telemetry ? process.memoryUsage() : null;
const started = process.hrtime.bigint();
const actual = run(args[0], args[1], n);
const elapsed = process.hrtime.bigint() - started;
const after = telemetry ? process.memoryUsage() : null;
if (telemetry) writeSync(1, 'GC_TELEMETRY_END\n');
verify(actual, step, roots, state);
const perGroup = args[0] === 'reference-cycle' ? 2 : args[0] === 'reference-array' ? 3 : 1;
const data = { runtime: 'node', node_version: process.version, v8_version: process.versions.v8,
  platform: process.platform, arch: process.arch, actual_exec_argv: process.execArgv,
  family: args[0], phase: args[1], iterations: n, warmup_iterations: warmN, warmup_rounds: rounds,
  execution_ns: Number(elapsed), step_checksum_u32: stepChecksum, root_checksum_u32: rootChecksum,
  last_lcg_u32: lastState, return_checksum_u32: actual, root_groups: ROOT_COUNT,
  table_root_slots: args[0] === 'reference-array' ? 2048 : 1024,
  planned_syntax_allocations: perGroup * (args[1] === 'allocate' ? n : ROOT_COUNT),
  collector_roi: false, gc_reclaimed_objects: null, gc_collection_count: null,
  physical_object_count_known: false, numeric_array_representation: 'Uint32Array + managed ArrayBuffer/backing storage',
  actual_heap_size_limit: getHeapStatistics().heap_size_limit };
if (telemetry) {
  data.memory_snapshot_before = before; data.memory_snapshot_after = after;
  data.memory_snapshot_is_allocated_byte_counter = false;
}
writeSync(1, JSON.stringify(data) + '\n');

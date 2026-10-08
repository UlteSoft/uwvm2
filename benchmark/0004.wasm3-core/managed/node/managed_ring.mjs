import {writeSync} from 'node:fs';

const seed = 123456789;
const a = 1664525;
const c = 1013904223;
const roots = new Array(1024);

class Box {
  constructor(value) { this.value = value; }
}

function run(count) {
  let state = seed;
  for (let remaining = count; remaining !== 0; --remaining) {
    state = (Math.imul(state, a) + c) | 0;
    const index = remaining & 1023;
    roots[index] = new Box(state);
    state = roots[index].value;
  }
  return state >>> 0;
}

function expected(count) {
  const mask = 0xffffffffn;
  let multiplier = 1n, increment = 0n;
  let powerMultiplier = BigInt(a), powerIncrement = BigInt(c);
  while (count !== 0) {
    if (count & 1) {
      multiplier = multiplier * powerMultiplier & mask;
      increment = (increment * powerMultiplier + powerIncrement) & mask;
    }
    powerIncrement = (powerMultiplier + 1n) * powerIncrement & mask;
    powerMultiplier = powerMultiplier * powerMultiplier & mask;
    count = Math.floor(count / 2);
  }
  return Number((multiplier * BigInt(seed) + increment) & mask);
}

function verifyRoots(count) {
  if (count < roots.length) throw new Error('need at least 1024 measured steps');
  let state = expected(count - roots.length) | 0;
  let expectedXor = 0;
  for (let i = 0; i < roots.length; ++i) {
    state = (Math.imul(state, a) + c) | 0;
    const index = (roots.length - i) & (roots.length - 1);
    if (!(roots[index] instanceof Box) || roots[index].value !== state)
      throw new Error(`root ring slot mismatch at ${index}`);
    expectedXor ^= state;
  }
  let observedXor = 0;
  for (const root of roots) {
    if (!(root instanceof Box)) throw new Error('missing live root');
    observedXor ^= root.value;
  }
  if (observedXor !== expectedXor) throw new Error('root ring mismatch');
  return observedXor >>> 0;
}

const count = Number(process.argv[2]);
const gcTelemetry = process.argv.length === 4 && process.argv[3] === '--gc-telemetry';
if (process.argv.length !== 3 && !gcTelemetry)
  throw new Error('expected one count and optional --gc-telemetry');
if (!Number.isSafeInteger(count) || count <= 0 || count > 0xffffffff)
  throw new Error('expected one positive u32 iteration count');
for (let i = 0; i < 8; ++i)
  if (run(250000) !== expected(250000)) throw new Error('warmup mismatch');
if (gcTelemetry) writeSync(1, 'GC_TELEMETRY_START\n');
const started = process.hrtime.bigint();
const actual = run(count);
const elapsed = process.hrtime.bigint() - started;
if (gcTelemetry) writeSync(1, 'GC_TELEMETRY_END\n');
if (actual !== expected(count)) throw new Error('result mismatch');
const rootChecksum = verifyRoots(count);
console.log(JSON.stringify({runtime: 'node', iterations: count,
                            guest_ns: Number(elapsed), checksum: actual,
                            root_checksum: rootChecksum}));

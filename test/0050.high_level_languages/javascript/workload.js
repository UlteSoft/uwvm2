// Javy embeds QuickJS in a statically linked core Wasm module with WASI imports.
// This is real JavaScript, including dynamic objects and catchable exceptions.
class Accumulator {
  constructor() { this.value = 0; }
  add(value) { this.value = (this.value + value) % 1000003; }
}

function workload(rounds) {
  const values = Array.from({ length: 64 }, (_, i) => (i * 7 + 3) % 97);
  const acc = new Accumulator();
  for (let round = 0; round < rounds; ++round) {
    for (let i = 0; i < values.length; ++i) {
      const index = (round + i) % values.length;
      values[index] = (values[index] * 17 + i + round % 13) % 997;
      acc.add(values[index] * (i % 11 + 1));
    }
  }
  return acc.value;
}

let caught = false;
try { throw new RangeError("expected"); }
catch (error) { caught = error instanceof RangeError && error.message === "expected"; }
if (!caught) throw new Error("exception conformance failed");
const label = ["uwvm", "high-level"].join(":");
console.log(`javascript ${label} checksum=${workload(20000)} caught=${caught}`);

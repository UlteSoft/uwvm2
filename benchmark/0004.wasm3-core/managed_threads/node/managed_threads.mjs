import {Worker, isMainThread, parentPort, workerData} from 'node:worker_threads';

// Cross-language analogue. Each Worker starts a V8 isolate, unlike a VM-managed
// host thread entering already compiled Wasm; report it separately.
const words = 1024;
const passes = 32;
const rotate = (value, bits) => ((value << bits) | (value >>> (32 - bits))) >>> 0;

function kernel(memory, control, worker) {
  Atomics.load(control, 0); // An atomic access on shared storage, not Wasm atomic.fence.
  const base = worker * words;
  const seed = 0x12345 + worker;
  for (let i = 0; i < words; ++i) memory[base + i] = (i + seed) >>> 0;
  for (let pass = 0; pass < passes; ++pass) {
    for (let i = 0; i < words; ++i) {
      const address = base + i;
      memory[address] = (rotate(memory[address], 7) ^ (seed + pass)) + 0x9e3779b9;
    }
  }
  let checksum = 0;
  for (let i = 0; i < words; ++i)
    checksum = (rotate(checksum, 5) + memory[base + i]) >>> 0;
  return checksum;
}

if (!isMainThread) {
  const result = kernel(new Uint32Array(workerData.memory),
                        new Int32Array(workerData.control), workerData.worker);
  parentPort.postMessage(result);
  parentPort.close();
} else {
  const samples = Number(process.argv[2]);
  const rounds = Number(process.argv[3]);
  const offset = Number(process.argv[4]);
  if (!Number.isInteger(samples) || samples < 1 || samples > 99 ||
      !Number.isInteger(rounds) || rounds < 1 || rounds > 1024 ||
      !Number.isInteger(offset) || offset < 0 || offset > 1)
    throw new Error('samples rounds order-offset required within bounds');
  const memoryBuffer = new SharedArrayBuffer(4 * words * Uint32Array.BYTES_PER_ELEMENT);
  const controlBuffer = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT);
  const memory = new Uint32Array(memoryBuffer);
  const control = new Int32Array(controlBuffer);
  const expected = Array.from({length: 4}, (_, worker) => kernel(memory, control, worker));

  function launch(worker) {
    return new Promise((resolve, reject) => {
      const task = new Worker(new URL(import.meta.url), {
        workerData: {memory: memoryBuffer, control: controlBuffer, worker}
      });
      let result;
      task.once('message', value => { result = value; });
      task.once('error', reject);
      task.once('exit', code => {
        if (code !== 0 || result === undefined) reject(new Error(`worker exited ${code}`));
        else resolve(result);
      });
    });
  }

  async function measure(threaded, workers, sample) {
    let wall = 0n;
    let checksum = 0;
    for (let round = 0; round < rounds; ++round) {
      const started = process.hrtime.bigint();
      const results = threaded
        ? await Promise.all(Array.from({length: workers}, (_, worker) => launch(worker)))
        : Array.from({length: workers}, (_, worker) => kernel(memory, control, worker));
      wall += process.hrtime.bigint() - started;
      for (let worker = 0; worker < workers; ++worker) {
        if (results[worker] !== expected[worker])
          throw new Error(`kernel checksum mismatch for worker ${worker}`);
        checksum += results[worker];
      }
    }
    if (sample >= 0)
      console.log(JSON.stringify({runtime: 'node-worker-isolate', workers, threaded,
        sample, rounds, wall_ns: Number(wall / BigInt(rounds)), checksum,
        requested_threads: threaded ? workers * rounds : 0}));
    return checksum;
  }

  for (let sample = -2; sample < samples; ++sample) {
    for (const workers of [1, 4]) {
      const firstThreaded = ((sample + offset) & 1) !== 0;
      await measure(firstThreaded, workers, sample);
      await measure(!firstThreaded, workers, sample);
    }
  }
}

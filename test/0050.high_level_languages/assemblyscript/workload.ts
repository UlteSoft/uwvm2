// AssemblyScript is a statically typed TypeScript-like language, not arbitrary JS.
// No JavaScript glue is required: the only import is WASI Preview 1 fd_write.
@external("wasi_snapshot_preview1", "fd_write")
declare function fd_write(fd: i32, iovs: usize, count: i32, written: usize): i32;

class Accumulator {
  value: i32 = 0;
  add(value: i32): void { this.value = (this.value + value) % 1000003; }
}

export function workload(rounds: i32): i32 {
  const values = new Array<i32>(64);
  for (let i = 0; i < values.length; ++i) values[i] = (i * 7 + 3) % 97;
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

function printLine(text: string): void {
  const bytes = String.UTF8.encode(text + "\n");
  const iov = memory.data(12);
  store<usize>(iov, changetype<usize>(bytes));
  store<i32>(iov + 4, bytes.byteLength);
  if (fd_write(1, iov, 1, iov + 8) != 0) unreachable();
}

export function _start(): void {
  const label = ["uwvm", "high-level"].join(":");
  printLine("assemblyscript " + label + " checksum=" + workload(20000).toString());
}

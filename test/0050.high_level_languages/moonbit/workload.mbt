///|
struct Accumulator {
  mut value : Int
}

///|
fn Accumulator::add(self : Accumulator, value : Int) -> Unit {
  self.value = (self.value + value) % 1000003
}

///|
pub fn workload(rounds : Int) -> Int {
  let values = Array::makei(64, fn(i) { (i * 7 + 3) % 97 })
  let acc = Accumulator::{ value: 0 }
  for round = 0; round < rounds; round = round + 1 {
    for i = 0; i < values.length(); i = i + 1 {
      let index = (round + i) % values.length()
      values[index] = (values[index] * 17 + i + round % 13) % 997
      acc.add(values[index] * (i % 11 + 1))
    }
  }
  acc.value
}

///|
suberror ExpectedError { ExpectedError(String) }

///|
fn throw_expected() -> Bool raise ExpectedError {
  raise ExpectedError("expected")
}

///|
fn fd_write(fd : Int, iovs : Int, count : Int, written : Int) -> Int =
  "wasi_snapshot_preview1" "fd_write"

///|
// Reserve bytes 0..31 for this ASCII-only test adapter. The linear backend's
// heap starts at 1024; the GC backend stores its managed objects outside memory.
extern "wasm" fn prepare_ascii_iovec(code : Int) -> Int =
  #|(func (param $code i32) (result i32)
  #|  (i32.store (i32.const 0) (i32.const 16))
  #|  (i32.store (i32.const 4) (i32.const 1))
  #|  (i32.store8 (i32.const 16) (local.get $code))
  #|  (i32.const 0))

///|
fn print_ascii_line(text : String) -> Unit {
  for char in text.iter() {
    let iov = prepare_ascii_iovec(char.to_int())
    if fd_write(1, iov, 1, 8) != 0 { abort("fd_write failed") }
  }
  let iov = prepare_ascii_iovec(10)
  if fd_write(1, iov, 1, 8) != 0 { abort("fd_write failed") }
}

///|
fn main {
  let caught = throw_expected() catch {
    ExpectedError(message) => message == "expected"
  }
  let label = "uwvm" + ":" + "high-level"
  print_ascii_line("moonbit \{label} checksum=\{workload(20000)} caught=\{caught}")
}

use std::io::{self, Read, Write};
use std::time::Instant;

#[inline(never)]
fn kernel(iterations: u32) -> u32 {
    let mut value = 7u32;
    for index in 0..iterations {
        value = value.wrapping_mul(33).wrapping_add(index) ^ 0x55aa;
    }
    value
}

fn fail(message: &str, status: i32) -> ! {
    eprintln!("lcg32 {message}");
    std::process::exit(status);
}

fn parse_iterations() -> u32 {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.is_empty() { return 2_000_000_000; }
    if args.len() != 1 || args[0].is_empty() || args[0].len() > 10
        || !args[0].bytes().all(|b| b.is_ascii_digit()) {
        fail("invalid iterations", 2);
    }
    match args[0].parse::<u32>() {
        Ok(value) if (1..=2_000_000_000).contains(&value) => value,
        _ => fail("invalid iterations", 2),
    }
}

fn marker(text: &[u8]) {
    let mut stderr = io::stderr().lock();
    if stderr.write_all(text).and_then(|_| stderr.flush()).is_err() {
        fail("marker output failed", 4);
    }
}

fn gate(wanted: u8) {
    let mut value = [0u8; 1];
    if io::stdin().read_exact(&mut value).is_err() || value[0] != wanted {
        fail("invalid gate", 3);
    }
}

fn main() {
    let iterations = parse_iterations();
    marker(b"UWVM_PROFILE_READY\n");
    gate(b'G');
    let start = Instant::now();
    let checksum = kernel(iterations);
    marker(b"UWVM_PROFILE_DONE\n");
    let elapsed = start.elapsed().as_nanos();
    gate(b'D');
    println!("lcg32 iterations={iterations} checksum={checksum}");
    println!("lcg32 kernel ns={elapsed}");
}

use std::env;
use std::fs::File;
use std::io::Read;
use std::path::PathBuf;
use std::time::Instant;

use varint_simd::{decode, decode_unsafe, VarIntTarget};

#[derive(Clone, Copy)]
struct Scenario<'a> {
    name: &'a str,
    kind: ScenarioKind,
}

#[derive(Clone, Copy)]
enum ScenarioKind {
    U8,
    U16,
}

fn read_env_size(name: &str, default_value: usize) -> usize {
    if let Ok(s) = env::var(name) {
        if let Ok(v) = s.parse::<u64>() {
            if v > 0 {
                return v as usize;
            }
        }
    }
    default_value
}

fn load_stream(data_dir: &str, scenario: &Scenario) -> (Vec<u8>, u64) {
    let mut path = PathBuf::from(data_dir);
    path.push(format!("{}.bin", scenario.name));

    let mut file = File::open(&path).unwrap_or_else(|e| {
        panic!(
            "failed to open data file {:?} for scenario {}: {}",
            path, scenario.name, e
        )
    });

    let mut header = [0u8; 8];
    file.read_exact(&mut header).unwrap_or_else(|e| {
        panic!(
            "failed to read header from {:?} for scenario {}: {}",
            path, scenario.name, e
        )
    });
    let func_count = u64::from_le_bytes(header);

    let mut bytes = Vec::new();
    file.read_to_end(&mut bytes).unwrap_or_else(|e| {
        panic!(
            "failed to read body from {:?} for scenario {}: {}",
            path, scenario.name, e
        )
    });

    // Ensure at least 16 bytes of padding for decode_unsafe.
    bytes.extend(std::iter::repeat(0u8).take(16));

    (bytes, func_count)
}

fn bench_decode_u8(bytes: &[u8], func_count: u64, iterations: usize) -> (u64, f64, f64) {
    let encoded_len = bytes.len().saturating_sub(16);
    let mut total_ns: u64 = 0;
    let mut total_bytes: u64 = 0;

    let mut sink: u64 = 0;

    for _ in 0..iterations {
        let mut slice = &bytes[..encoded_len];
        let t0 = Instant::now();
        for _ in 0..func_count {
            // SAFETY: `bytes` has at least 16 bytes of padding at the end.
            let (num, len) = unsafe { decode_unsafe::<u8>(slice.as_ptr()) };
            sink = sink.wrapping_add(num as u64);
            slice = &slice[len as usize..];
        }
        let elapsed = t0.elapsed();
        total_ns = total_ns.saturating_add(elapsed.as_nanos() as u64);
        total_bytes = total_bytes.saturating_add(encoded_len as u64);
    }

    // Prevent the compiler from optimizing away the decode loop.
    std::hint::black_box(sink);

    let total_values = func_count * iterations as u64;
    let ns_per_value = total_ns as f64 / total_values as f64;
    let avg_bytes_per_value = total_bytes as f64 / total_values as f64;

    (total_ns, ns_per_value, avg_bytes_per_value)
}

fn bench_decode_u16(bytes: &[u8], func_count: u64, iterations: usize) -> (u64, f64, f64) {
    let encoded_len = bytes.len().saturating_sub(16);
    let mut total_ns: u64 = 0;
    let mut total_bytes: u64 = 0;

    let mut sink: u64 = 0;

    for _ in 0..iterations {
        let mut slice = &bytes[..encoded_len];
        let t0 = Instant::now();
        for _ in 0..func_count {
            // SAFETY: `bytes` has at least 16 bytes of padding at the end.
            let (num, len) = unsafe { decode_unsafe::<u16>(slice.as_ptr()) };
            sink = sink.wrapping_add(num as u64);
            slice = &slice[len as usize..];
        }
        let elapsed = t0.elapsed();
        total_ns = total_ns.saturating_add(elapsed.as_nanos() as u64);
        total_bytes = total_bytes.saturating_add(encoded_len as u64);
    }

    std::hint::black_box(sink);

    let total_values = func_count * iterations as u64;
    let ns_per_value = total_ns as f64 / total_values as f64;
    let avg_bytes_per_value = total_bytes as f64 / total_values as f64;

    (total_ns, ns_per_value, avg_bytes_per_value)
}

fn bench_decode_u8_safe(bytes: &[u8], func_count: u64, iterations: usize) -> (u64, f64, f64) {
    let encoded_len = bytes.len().saturating_sub(16);
    let mut total_ns: u64 = 0;
    let mut total_bytes: u64 = 0;

    let mut sink: u64 = 0;

    for _ in 0..iterations {
        let mut slice = &bytes[..encoded_len];
        let t0 = Instant::now();
        for _ in 0..func_count {
            let (num, len) = decode::<u8>(slice).expect("decode::<u8> failed");
            sink = sink.wrapping_add(num as u64);
            slice = &slice[len as usize..];
        }
        let elapsed = t0.elapsed();
        total_ns = total_ns.saturating_add(elapsed.as_nanos() as u64);
        total_bytes = total_bytes.saturating_add(encoded_len as u64);
    }

    std::hint::black_box(sink);

    let total_values = func_count * iterations as u64;
    let ns_per_value = total_ns as f64 / total_values as f64;
    let avg_bytes_per_value = total_bytes as f64 / total_values as f64;

    (total_ns, ns_per_value, avg_bytes_per_value)
}

fn bench_decode_u16_safe(bytes: &[u8], func_count: u64, iterations: usize) -> (u64, f64, f64) {
    let encoded_len = bytes.len().saturating_sub(16);
    let mut total_ns: u64 = 0;
    let mut total_bytes: u64 = 0;

    let mut sink: u64 = 0;

    for _ in 0..iterations {
        let mut slice = &bytes[..encoded_len];
        let t0 = Instant::now();
        for _ in 0..func_count {
            let (num, len) = decode::<u16>(slice).expect("decode::<u16> failed");
            sink = sink.wrapping_add(num as u64);
            slice = &slice[len as usize..];
        }
        let elapsed = t0.elapsed();
        total_ns = total_ns.saturating_add(elapsed.as_nanos() as u64);
        total_bytes = total_bytes.saturating_add(encoded_len as u64);
    }

    std::hint::black_box(sink);

    let total_values = func_count * iterations as u64;
    let ns_per_value = total_ns as f64 / total_values as f64;
    let avg_bytes_per_value = total_bytes as f64 / total_values as f64;

    (total_ns, ns_per_value, avg_bytes_per_value)
}

fn main() {
    let data_dir = env::var("FS_BENCH_DATA_DIR").unwrap_or_else(|_| {
        panic!("FS_BENCH_DATA_DIR is not set; it must point to the shared data directory")
    });

    let iterations = read_env_size("ITERS", 20);

    let scenarios = [
        Scenario {
            name: "u8_1b",
            kind: ScenarioKind::U8,
        },
        Scenario {
            name: "u8_2b",
            kind: ScenarioKind::U8,
        },
        Scenario {
            name: "u16_2b",
            kind: ScenarioKind::U16,
        },
    ];

    println!("varint-simd fair decode benchmark (shared LEB128 streams)");
    println!("  data_dir   = {}", data_dir);
    println!("  iterations = {}", iterations);

    for scenario in &scenarios {
        let (bytes, func_count) = load_stream(&data_dir, scenario);

        let (total_ns_unsafe, ns_per_value_unsafe, avg_bytes_per_value_unsafe) = match scenario.kind {
            ScenarioKind::U8 => bench_decode_u8(&bytes, func_count, iterations),
            ScenarioKind::U16 => bench_decode_u16(&bytes, func_count, iterations),
        };

        let total_values = func_count * iterations as u64;
        let total_bytes_unsafe = avg_bytes_per_value_unsafe * total_values as f64;
        let seconds_unsafe = total_ns_unsafe as f64 * 1e-9;
        let gib_per_s_unsafe = (total_bytes_unsafe / (1024.0 * 1024.0 * 1024.0)) / seconds_unsafe;

        println!(
            "varint_simd_fs scenario={} impl=unsafe values={} total_ns={} \
ns_per_value={:.6} avg_bytes_value={:.6} gib_per_s={:.6}",
            scenario.name,
            total_values,
            total_ns_unsafe,
            ns_per_value_unsafe,
            avg_bytes_per_value_unsafe,
            gib_per_s_unsafe
        );

        let (total_ns_safe, ns_per_value_safe, avg_bytes_per_value_safe) = match scenario.kind {
            ScenarioKind::U8 => bench_decode_u8_safe(&bytes, func_count, iterations),
            ScenarioKind::U16 => bench_decode_u16_safe(&bytes, func_count, iterations),
        };

        let total_bytes_safe = avg_bytes_per_value_safe * total_values as f64;
        let seconds_safe = total_ns_safe as f64 * 1e-9;
        let gib_per_s_safe = (total_bytes_safe / (1024.0 * 1024.0 * 1024.0)) / seconds_safe;

        println!(
            "varint_simd_fs scenario={} impl=safe values={} total_ns={} \
ns_per_value={:.6} avg_bytes_value={:.6} gib_per_s={:.6}",
            scenario.name,
            total_values,
            total_ns_safe,
            ns_per_value_safe,
            avg_bytes_per_value_safe,
            gib_per_s_safe
        );
    }
}

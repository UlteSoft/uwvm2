// uwvm-dbg-pretty-rust-native-v2: unchanged real Rust producer, no synthetic DWARF.
#![allow(improper_ctypes_definitions)]
#[inline(never)]
#[no_mangle]
pub extern "C" fn pretty_rust_ready(values: &Vec<i32>, title: &String, slice: &[i32], text: &str, words: &Vec<String>, units: &Vec<()>) {
    // UWVM_RUST_PRETTY_STOP
    std::hint::black_box((values.len(), title.len(), slice.len(), text.len(), words.len(), units.len()));
}
fn main() {
    let mut values = Vec::<i32>::with_capacity(32);
    for i in 0..16 { values.push(i * 3 + 1); }
    let title = String::from("native Rust \u{03bb} printer");
    let slice = &values[2..6];
    let text: &str = title.as_str();
    let words = vec![String::from("first"), String::from("second"), String::from("\u{03bb}")];
    let mut units = Vec::<()>::new();
    units.resize(7, ());
    pretty_rust_ready(&values, &title, slice, text, &words, &units);
    assert_eq!(words.len(), 3);
    assert_eq!(words[2], "\u{03bb}");
    assert_eq!(units.len(), 7);
    assert_eq!(units.capacity(), usize::MAX);
    assert_eq!(values.len(), 16);
    assert_eq!(values.capacity(), 32);
    assert_eq!(values.iter().copied().sum::<i32>(), 376);
    assert_eq!(slice, &[7, 10, 13, 16]);
    assert_eq!(text, "native Rust \u{03bb} printer");
}

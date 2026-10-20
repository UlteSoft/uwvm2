// Genuine Rust standard-library VecDeque producer; no synthetic DWARF.
#![allow(improper_ctypes_definitions)]
use std::collections::VecDeque;
#[inline(never)]
#[no_mangle]
pub extern "C" fn pretty_rust_deque_ready(values: &VecDeque<i32>, words: &VecDeque<String>, units: &VecDeque<()>) {
    // UWVM_RUST_DEQUE_STOP
    std::hint::black_box((values.len(), words.len(), units.len()));
}
fn main() {
    let mut values = VecDeque::<i32>::with_capacity(8);
    for i in 0..8 { values.push_back(i); }
    for _ in 0..6 { values.pop_front(); }
    for i in 8..14 { values.push_back(i); }
    let mut words = VecDeque::<String>::with_capacity(4);
    for word in ["old0", "old1", "old2", "first"] { words.push_back(String::from(word)); }
    for _ in 0..3 { words.pop_front(); }
    for word in ["second", "\u{03bb}", "last"] { words.push_back(String::from(word)); }
    let mut units = VecDeque::<()>::new();
    for _ in 0..7 { units.push_front(()); }
    pretty_rust_deque_ready(&values, &words, &units);
    assert_eq!(values.iter().copied().collect::<Vec<_>>(), (6..14).collect::<Vec<_>>());
    assert_eq!(values.capacity(), 8);
    assert_eq!(values.as_slices().0.len(), 2);
    assert_eq!(words.iter().map(String::as_str).collect::<Vec<_>>(), ["first", "second", "\u{03bb}", "last"]);
    assert_eq!(words.as_slices().0.len(), 1);
    assert_eq!(units.len(), 7);
    assert_eq!(units.capacity(), usize::MAX);
}

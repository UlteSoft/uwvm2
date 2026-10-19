use std::collections::BTreeMap;

trait Accumulator {
    fn add(&mut self, amount: i32) -> i32;
}
struct Counter { value: i32 }
impl Accumulator for Counter {
    fn add(&mut self, amount: i32) -> i32 {
        self.value += amount;
        self.value
    }
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn debug_value() -> i32 {
    let mut value = 40;
    value += 2; // DEBUG_BEFORE
    value // DEBUG_AFTER
}

fn kernel(iterations: u32) -> u32 {
    let mut value = 7u32;
    for index in 0..iterations {
        value = value.wrapping_mul(33).wrapping_add(index) ^ 0x55aa;
    }
    value
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let iterations = match args.as_slice() {
        [] => Some(1000),
        [text] => text.parse::<u32>().ok().filter(|v| (1..=1_000_000).contains(v)),
        _ => None,
    };
    let Some(iterations) = iterations else {
        eprintln!("rust invalid iterations");
        std::process::exit(2);
    };
    let mut object: Box<dyn Accumulator> = Box::new(Counter { value: 40 });
    let classes = object.add(2);
    let numbers: Vec<i32> = (1..=10).collect();
    let arrays: i32 = numbers.iter().sum();
    let records = BTreeMap::from([("left", 1), ("right", 5)]);
    let record_sum = records["left"] + records["right"];
    let text = "π你好🙂";
    let checks = u32::from("bad".parse::<i32>().is_err())
        + u32::from(numbers.get(classes as usize).is_none())
        + u32::from(i32::MAX.checked_add(1).is_none());
    let value = debug_value();
    assert_eq!((classes, arrays, record_sum, text.chars().count(), text.len(), checks, value),
               (42, 55, 6, 4, 12, 3, 42));
    println!("rust classes={classes} arrays={arrays} records={record_sum} unicode=4 utf8=12 checks={checks} value={value} iterations={iterations} checksum={}", kernel(iterations));
}

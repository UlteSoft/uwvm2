// Compiler witnesses for the bounded debugger Rust char contract.
const _: () = {
 assert!('a' as u32 == 97);assert!('\n' as u32 == 10);assert!('\0' as u32 == 0);
 assert!('\'' as u32 == 39);assert!('\\' as u32 == 92);assert!('\"' as u32 == 34);
 assert!('\x7f' as u32 == 127);assert!('\u{3bb}' as u32 == 955);
 assert!('\u{1f642}' as u32 == 128578);assert!('\u{10_ffff__}' as u32 == 0x10ffff);
 assert!('\u{1f642}' as u8 == 66);assert!('\u{1f642}' as i8 == 66);
 assert!('\u{3bb}' as i32 == 955);assert!(255u8 as char == '\u{ff}');
 assert!(65u8 as char == 'A');assert!('a' as char == 'a');
 assert!('\u{3bb}' < '\u{1f642}');assert!('a'=='a');assert!('a'!='b');assert!(!('a'>='b'));
 assert!(('a' as u32)+1u32 == 98);assert!(core::mem::size_of::<char>() == 4);
};

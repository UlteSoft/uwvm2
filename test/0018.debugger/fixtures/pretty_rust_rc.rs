// Genuine Rust standard-library ownership producer, with live/empty/expired weak values.
#![allow(improper_ctypes_definitions)]
use std::rc::{Rc,Weak};
use std::sync::{Arc,Weak as SyncWeak};
#[inline(never)]
#[no_mangle]
pub extern "C" fn pretty_rust_rc_ready(number: &Rc<i32>, text: &Rc<String>, unit: &Rc<()>, atomic: &Arc<i32>, numbers: &Arc<Vec<i32>>, atomic_unit: &Arc<()>, weak: &Weak<i32>, expired: &Weak<i32>, empty: &Weak<i32>, atomic_weak: &SyncWeak<i32>, atomic_expired: &SyncWeak<i32>, atomic_empty: &SyncWeak<i32>) {
    // UWVM_RUST_RC_STOP
    std::hint::black_box((number,text,unit,atomic,numbers,atomic_unit,weak,expired,empty,atomic_weak,atomic_expired,atomic_empty));
}
fn main() {
    let number=Rc::new(-17); let clone1=number.clone(); let clone2=number.clone();
    let weak=Rc::downgrade(&number); let weak2=weak.clone();
    let text=Rc::new(String::from("rc-λ")); let unit=Rc::new(());
    let atomic=Arc::new(42); let atomic_clone=atomic.clone(); let atomic_weak=Arc::downgrade(&atomic);
    let numbers=Arc::new(vec![4,9]); let numbers_clone=numbers.clone(); let atomic_unit=Arc::new(());
    let expired={let value=Rc::new(99);Rc::downgrade(&value)};let empty=Weak::<i32>::new();
    let atomic_expired={let value=Arc::new(100);Arc::downgrade(&value)};let atomic_empty=SyncWeak::<i32>::new();
    pretty_rust_rc_ready(&number,&text,&unit,&atomic,&numbers,&atomic_unit,&weak,&expired,&empty,&atomic_weak,&atomic_expired,&atomic_empty);
    assert_eq!(*number,-17);assert_eq!(Rc::strong_count(&number),3);assert_eq!(Rc::weak_count(&number),2);
    assert_eq!(&**text,"rc-λ");assert_eq!(*atomic,42);assert_eq!(Arc::strong_count(&atomic),2);assert_eq!(Arc::weak_count(&atomic),1);
    assert_eq!(&**numbers,&[4,9]);assert!(expired.upgrade().is_none() && empty.upgrade().is_none());
    assert!(atomic_expired.upgrade().is_none() && atomic_empty.upgrade().is_none());
    std::hint::black_box((clone1,clone2,weak2,atomic_clone,numbers_clone));
}

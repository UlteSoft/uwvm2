;; A provider owns an exnref global; consumers borrow its complete 16-byte
;; reference value through import linking without enabling GC/function refs.
(module
  (global $saved (export "saved") (mut (ref null exn)) (ref.null noexn)))

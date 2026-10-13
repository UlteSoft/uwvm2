(module
  (table (export "t_exn") 0 exnref)
  (table (export "t_noexn") 0 (ref null noexn))
  (global (export "g_exn") exnref (ref.null exn))
  (global (export "g_noexn") (ref null noexn) (ref.null noexn))
  (func (export "f_exn") (param exnref))
  (func (export "f_noexn") (param (ref null noexn))))

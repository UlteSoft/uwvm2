;; In-range relaxed swizzle indices have a deterministic SIMD result.
(module
  (func (export "_start")
    v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
    v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 0
    i8x16.relaxed_swizzle
    i8x16.extract_lane_u 0
    i32.const 1
    i32.ne
    if unreachable end))

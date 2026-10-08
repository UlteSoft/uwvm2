(module
  (import "provider64-host" "mem" (memory $mem i64 1 1))
  (data $keep "\0a\14\1e\28\32\3c\46\50")
  (data $drop "\11\22\33\44")
  ;; 0: genuine host-provider64 init/fill, including the 4096-byte staging edge.
  (func (export "valid")
    i64.const 32 i32.const 0 i32.const 8 memory.init $mem $keep
    i64.const 22000 i32.const 421 i64.const 5000 memory.fill $mem
    i64.const 65536 i32.const 255 i64.const 0 memory.fill $mem
    i64.const 65536 i32.const 8 i32.const 0 memory.init $mem $keep)
  ;; 1: two idempotent drops and empty dropped-data init at the memory end.
  (func (export "drop-empty")
    data.drop $drop data.drop $drop
    i64.const 65536 i32.const 0 i32.const 0 memory.init $mem $drop)
  ;; 2/3: high destination MUST NOT narrow to native/i32 zero, even with len=0.
  (func (export "fill-high-zero")
    i64.const 4294967296 i32.const 165 i64.const 0 memory.fill $mem)
  (func (export "init-high-zero")
    i64.const 4294967296 i32.const 0 i32.const 0 memory.init $mem $keep)
  ;; 4/5: complete range validation must precede the first provider write.
  (func (export "fill-cross-end")
    i64.const 65535 i32.const 165 i64.const 2 memory.fill $mem)
  (func (export "init-cross-end")
    i64.const 65535 i32.const 0 i32.const 2 memory.init $mem $keep)
  ;; 6: zero length does not waive the data-source bound.
  (func (export "init-source-zero")
    i64.const 0 i32.const 9 i32.const 0 memory.init $mem $keep)
  ;; 7: actual data.drop makes a formerly nonempty source empty.
  (func (export "init-dropped")
    data.drop $drop
    i64.const 0 i32.const 0 i32.const 1 memory.init $mem $drop)
  ;; 8: full i64 length, unlike memory.init's i32 source and length.
  (func (export "fill-high-length")
    i64.const 0 i32.const 165 i64.const 4294967296 memory.fill $mem)
  ;; 9/10: unsigned i64 endpoints, including overflow-like mathematical sums.
  (func (export "init-u64-max-zero")
    i64.const -1 i32.const 0 i32.const 0 memory.init $mem $keep)
  (func (export "fill-u64-max-one")
    i64.const -1 i32.const 165 i64.const 1 memory.fill $mem))

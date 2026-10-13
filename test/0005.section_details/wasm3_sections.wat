;; Core 3 parser-only fixture: large declared limits are metadata, not allocations.
;; https://webassembly.github.io/spec/core/text/types.html
(module
  (rec
    (type $node (struct (field (mut i8)) (field (ref null $node))))
    (type $array (array (mut i16))))
  (type $callback (func (param (ref null $node) v128) (result (ref $array))))
  (type $payload (func (param (ref null $node))))
  (import "host" "e" (tag (type $payload)))
  (import "host" "t" (table i64 4294967296 4294967297 (ref null $callback)))
  (import "host" "m" (memory i64 4294967296 4294967297))
  (import "host" "g" (global (mut (ref null $node))))
  (import "host" "n" (global (ref $node)))
  (table i64 4294967296 4294967297 (ref null $callback))
  (memory i64 4294967296 4294967297)
  (tag $local1 (type $payload))
  (tag $local2 (type $payload))
  (global $root (ref null $node) (ref.null $node))
  (elem (ref null $node) (ref.null $node)))

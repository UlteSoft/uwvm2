;; Same carrier and arity as the provider, but a different aggregate shape.
;; Linking must reject this rather than compare only the projected funcref bytes.
(module
  (type $record (struct (field i64)))
  (type $make (func (result (ref $record))))
  (import "P" "make" (func $make (type $make)))
  (func (export "_start")
    call $make
    drop))

;; Actual provider SOURCE. Must really be assembled and validated on Linux.
;; Its memory limits match the declared compiler recipe; no host memory fake.
(module
  (memory (export "memory") 256 1024 shared))

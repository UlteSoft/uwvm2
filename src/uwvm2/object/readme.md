# WebAssembly object

* When using the object, the template type of the parser must be downgraded. Because it must be forward compatible.

* When using `object`, the parser is downgraded via the template, making it type-safe.

## Memory model

* For an overview of the host memory models and linear memory backends used by the object layer, see [memory/readme.h](memory/readme.h).
## UWVM Debug Interpreter

The debug interpreter provides **full WebAssembly (Wasm) semantic coverage** and is designed for precise and controllable execution analysis.

### Features

- **Complete Wasm Semantics**  
  Fully implements the WebAssembly specification, ensuring strict adherence to official Wasm execution semantics.

- **Comprehensive Breakpoint Support**  
  Supports breakpoints at all interruptible points defined by the Wasm execution model, enabling fine-grained execution control.

- **State Introspection at Semantic Step Granularity**  
  Exposes all execution states at each step defined by Wasm semantics, making the complete program state observable and debuggable.

- **Hot Replacement of Wasm Code with Revalidation**  
  Allows replacement of Wasm code blocks during debugging. Any modified block is revalidated before execution resumes to guarantee semantic correctness and safety.

- **Compilation Model Limitation**  
  The debug interpreter supports **full compilation only** and does **not** support lazy compilation.

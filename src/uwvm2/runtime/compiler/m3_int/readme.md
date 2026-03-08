# M3 Interpreter

The **M3 Interpreter** is a C++ port of **`m3`** from **wasm3**. It is designed as an alternative interpreter backend for **uwvm2**.

## Background

wasm3 is widely recognized for being both **lightweight** and **high-performance**. Its compact design makes it especially attractive for environments where simplicity, portability, and low overhead matter, while still delivering strong interpreter performance.

Because of these qualities, we chose to port wasm3’s `m3` into C++ and integrate it as a candidate interpreter for uwvm2.

## Design

In the original wasm3 implementation, interpreter opcode handler functions are generated through **macros**.  
In this C++ port, that mechanism has been refactored to use **templates** instead.

This change provides several benefits:

- **Better type safety**
- **Improved readability and maintainability**
- **Cleaner abstraction for opcode generation**
- **Stronger integration with modern C++ codebases**

## Goal

The goal of the M3 Interpreter is to preserve the core advantages of wasm3:

- **small footprint**
- **efficient execution**
- **simple interpreter architecture**

while making the implementation better suited for the uwvm2 ecosystem through a more idiomatic C++ design.

## Position in uwvm2

Within uwvm2, the M3 Interpreter serves as a **backup / alternative interpreter implementation**.  
It offers a practical option when a lightweight interpreter with solid runtime performance is preferred.

## Summary

In short, the M3 Interpreter is:

- a **C++ port** of wasm3’s `m3`
- a **template-based redesign** of macro-generated opcode handlers
- a **lightweight and performant interpreter option** for uwvm2

It combines wasm3’s proven interpreter model with a C++-oriented implementation approach, making it a strong candidate as an alternative execution engine in uwvm2.
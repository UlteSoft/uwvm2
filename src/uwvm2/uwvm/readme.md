# UWVM command-line program
Providing a UWVM command line program

* `main.cpp` main function for linking
* `cmdline` uwvm program-specialized command line processing
* `crtmain` Stores C++ global variables for raii control, C++ utf-8 main functions, and special command line parsing on windows NT.
* `custom` User-defined structures such as version information
* `io` Provides interfaces to IO settings, (void*) `handle` on NT and Win32, (int) `fd 0 1 2` on POSIX, (FILE*) `stdin` `stdout` `stderr` on AvrLibc.
* `run` Run the wasm program
* `utils` Some other miscellaneous items
* `wasm` Storing wasm-related stuff

# Caveat
Since the uwvm command-line program is an independent program without external dependencies, global variables are allowed.

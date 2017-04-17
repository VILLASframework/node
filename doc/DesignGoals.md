# Design Goals {#designgoals}

VILLASnode ...

- is written in C the programming language
- is using features of the C11 standard
- is using an object oriented programming paradigm
- can be compiled with Clang / LLVM or GCC
- is released under the LGPLv2 license
- only relies on open source software libraries and the Linux kernel
- is extensible with new node types & hooks
- is heavily multi-threaded
- follows the Unix philosophy
- is separated into a library (libvillas) and a few binaries (villas-server, villas-pipe, villas-test-*, villas-signal, villas-hook) which link against the lib.
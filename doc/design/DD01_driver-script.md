# driver script

The driver needs to support a number of things simulataensly in a clean and simple user interface.

- compile a set of `.pebl` files into an executable
- compile a single `.pebl` file into an object file
- compile a single `.pebl` file into an asm file
- control how optimized a binary is
  - ie `-opt=[none|basic|full]`
  - `basic` for now is just `mem2reg`, but maybe more in the future?
  - `full` is `-O3`
- link a set of object files into an executable
- buitd a set of `.pebl` files into a library
  - likely a simple standalone `.a`
- all of the above requires similar support for the runtime, the stdlib, and main
  - either should be able to come from a source root directory, or an archive

## runtime/stdlib/main rationale

the driver will prefer a precompiled runtime/stdlib/main, which resides in an archive.

It will find both of these by searching first `pebl-install/lib/runtime` and `pebl-install/lib/stdlib` directories (this will require a change in the current install logic. `pebl-install/lib` used to contain the compiler libraries, those will now be in `pebl-install/lib/compiler`). Then it will seach `PATH`. Any user defined path will override this.

## flags

- by default, build combine all source files with runtime, stdlib and main
- `-c` - compile `.pebl` to `.o`
- `--library` - run the full compiler, but dont link against `main` 
- `--path PATH` - adds the specifed diretcory to the front of the search path, NOT recursive

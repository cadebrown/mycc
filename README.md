# mycc - (subset of C) Compiler

author: [Cade Brown <me@cade.site>](mailto:me@cade.site), re-implemented UTK's 'csem' compiler lab with modern C++


Check out `extras/` to see the kinds of projects it can compile!

This project describes a C compiler which can compile a decent subset of C, including:

  * Function definitions
  * Function declarations
  * Declarations
  * `int`, `char`, `double` types
  * Pointer types
  * Array types
  * `if`, `while`, `do...while`, `for`, and `goto` (with labels)
  * Most operators
  * Integer, floating point, and string literals

To do this, we use:

  * Yacc/Bison (see src/gram.y for grammar)
  * Flex/Lex (see src/lex.l for lexer)
  * LLVM (most files use this for code generation)


Additionally, if compiled with `USE_ORC`, it runs as a JIT compiler (Just In Time), and runs your program.

It's actually suprisingly useful, since you can declare functions with extern (like `extern double sin(double x);`), and the JIT function will link them from the running process.


## Building

For dependencies:

```shell
$ sudo apt install bison yacc flex
```

For students, compiling the project should only require implementing source code in `src/sem.cc`. From there, you should be able to compile via `make`. The resulting binary (`./mycc`) can be read and fed C source code on stdin.

## Running

Try:

```shell
$ make
$ ./mycc < extras/hello_world.c
```

Notice, mycc works on its stdin, and prints the LLVM IR as well as the JIT'd output

## Limitations

There are a few limitations to this compiler, some of which are very cumbersome. For example, you must have a return statement in every function (even though technically, C has a special case for the `main` function to return 0 by default). Further, having several return statements in a row is malformed and might cause errors.

## From `csem`

This was originally adapted and rewritten with `csem` as the base. That code had some leaky abstractions (`struct sem_rec`) that were left over after it was ported to LLVM. I've done away with those. And, further, I've replaced hashtable/other datastructures with the C++11 equivalents (`std::unorded_map`, etc).

The biggest improvement is the proper LLVM types -- pretty much all the code uses LLVM types, and those that don't use C++ STL primitives in conjunction with them.


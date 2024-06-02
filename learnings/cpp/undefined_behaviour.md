# Undefined Behaviour

Undefined behaviour is the result of doing something that is not well-defined by the C++ language.

The concrete consequence of undefined behaviour can be things like:

- Your program produces different results every time it is run.
- Your program consistently produces the same incorrect result.
- Your program behaves inconsistently (sometimes gives the correct result, other times not).
- Your program seems like its working but produces incorrect results later in the program.
- Your program crashes.
- Your program works on some compilers but not others.
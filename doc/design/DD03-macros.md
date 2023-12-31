# Macros

As a step towards true modularization, there is a need for the ability to call other functions from the stdlib. However, the code needs to know what functions/types exist. A quick fix is to `$include` a header definition (like C). Below is an example of a few key macro system needs (inspired heavily by C).

```pebl

# single line replacement
$define foo bar

# multi line macro
$macro foo(a, b, c)
a + b + c
$endmacro

# include directives
$include "path/to/file"

# if statement
$if expr

$else

$endif


# if statement
$ifdef INCLUDE_GUARD
$define INCLUDE_GUARD

$endif

```

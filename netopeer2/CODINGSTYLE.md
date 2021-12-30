# Netopeer2 Coding Style

This file describes the coding style used in most C files in the Netopeer2
tools.

## Basics

- Use space instead of tabs for indentations.
- There is no strict limit for the line length, However, try to keep lines in a
reasonable length (120 characters).
- Avoid trailing spaces on lines.
- Put one blank line between function definitions.
- Don't mix declarations and code within a block. Similarly, don't use
  declarations in iteration statements.

## Naming

Use underscores to separate words in an identifier: `multi_word_name`. 

Use lowercase for most names. Use uppercase for macros, macro parameters and
members of enumerations.

Do not use names that begin with `_`. If you need a name for "internal use
only", use `__` as a suffix instead of a prefix.

## Comments

Avoid `//` comments. Use `/* ... */` comments, write block comments with the
leading asterisk on each line. You may put the `/*` and `*/` on the same line as
comment text if you prefer.

```c
/*
 * comment text
 */
```

## Functions

Put the return type, function name, and the braces that surround the function's
code on separate lines, all starting in column 0.

```c
static int
foo(int arg)
{
    ...
}
```

When you need to put the function parameters on multiple lines, start new line
at column after the opening parenthesis from the initial line.

```c
static int
my_function(struct my_struct *p1, struct another_struct *p2,
            int size)
{
    ...
}
```

In the absence of good reasons for another order, the following parameter order
is preferred. One notable exception is that data parameters and their
corresponding size parameters should be paired.

1. The primary object being manipulated, if any (equivalent to the "this"
   pointer in C++).
2. Input-only parameters.
3. Input/output parameters.
4. Output-only parameters.
5. Status parameter.

Functions that destroy an instance of a dynamically-allocated type should accept
and ignore a null pointer argument. Code that calls such a function (including
the C standard library function `free()`) should omit a null-pointer check. We
find that this usually makes code easier to read.

### Function Prototypes

Put the return type and function name on the same line in a function prototype:

```c
static const struct int foo(int arg);
```

## Statements

- Indent each level of code with 4 spaces.
- Put single space between `if`, `while`, `for`, etc. statements and the
  expression that follow them. On the other hand, function calls has no space
  between the function name and opening parenthesis.
- Opening code block brace is kept at the same line with the `if`, `while`,
  `for` or `switch` statements.

```c
if (a) {
    x = exp(a);
} else {
    return 1;
}
```

- Start switch's cases at the same column as the switch.

```c
switch (conn->state) {
case 0:
    return "data found";
case 1:
    return "data not found";
default:
    return "unknown error";
}
```

- Do not put gratuitous parentheses around the expression in a return statement,
that is, write `return 0;` and not `return(0);`

## Types

Use typedefs sparingly. Code is clearer if the actual type is visible at the
point of declaration. Do not, in general, declare a typedef for a struct, union,
or enum. Do not declare a typedef for a pointer type, because this can be very
confusing to the reader.

Use the `int<N>_t` and `uint<N>_t` types from `<stdint.h>` for exact-width
integer types. Use the `PRId<N>`, `PRIu<N>`, and `PRIx<N>` macros from
`<inttypes.h>` for formatting them with `printf()` and related functions.

Pointer declarators bind to the variable name, not the type name. Write
`int *x`, not `int* x` and definitely not `int * x`.

## Expresions

Put one space on each side of infix binary and ternary operators:

```c
* / % + - << >> < <= > >= == != & ^ | && || ?: = += -= *= /= %= &= ^= |= <<= >>=
```

Do not put any white space around postfix, prefix, or grouping operators with
one exception - `sizeof`, see the note below.

```c
() [] -> . ! ~ ++ -- + - * &
```

The "sizeof" operator is unique among C operators in that it accepts two very
different kinds of operands: an expression or a type. In general, prefer to
specify an expression
```c
int *x = calloc(1, sizeof *x);
```
When the operand of sizeof is an expression, there is no need to parenthesize
that operand, and please don't. There is an exception to this rule when you need
to work with partially compatible structures:

```c
struct a_s {
   uint8_t type;
}

struct b_s {
   uint8_t type;
   char *str;
}

struct c_s {
   uint8_t type;
   uint8_t *u8;
}
...
struct a_s *a;

switch (type) {
case 1:
    a = (struct a_s *)calloc(1, sizeof(struct b_s));
    break;
case 2:
    a = (struct a_s *)calloc(1, sizeof(struct c_s));
    break;
    ...
```

# C++ Style Guide
The goal of this document is to describe our C++ style convention. Our
coding style generally follows
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
This document highlights core coding style.

## Header Files
### `#define` Guard

Use `#define` directive in all header files to prevent multiple
inclusion of header files. The format of the identifier name should be
`__[Project Name][HeaderFileName]__`. For example, for
`MyVeryCoolClass.h` in `Escargot` project, the guard should be
`__EscargotMyVeryCoolClass__` .


```cpp
#ifndef __EscargotMyVeryCoolClass__
#define __EscargotMyVeryCoolClass__
...
#endif
```

### Use `#include` only in `.cpp` files
To prevent possible loops in header file inclusion, include header files only
in `.cpp` files. In this case, the order of header file inclusion is important.

## Formatting
### Indentation
Use spaces only, and use 4 spaces at a time. Tabs should not be used.

### Empty Lines between Functions
Add one empty line between function implementations in `.cpp` files.
Add zero lines between function declarations (or implementations) in `.h` files
to explicitly group related functions. Add one empty line to separate between
these groups.

### `if` Statements
Add a space before the opening parenthesis, and between closing parenthesis and
opening curly brace. Always enclose statements with a pair of braces even for
a single line statement.

```cpp
if (condition) {
    ...
}

if (condition) {
    a = b;
}
```

Write the condition of `if` scope with explicit expressions. For details, please refer to [Code readability](#Code-readability)

### `for`, `while`, `do-while` Statements
Add a space before the opening parenthesis, and between closing parenthesis and
opening curly brace. Always have statements with a pair of braces even for a
single line statement.

```cpp
while (condition) {
    ...
}

while (condition) {
    a = b;
}
```

Write the condition of `for`, `while`, and `do-while` scopes with explicit expressions. For details, please refer to [Code readability](#Code-readability)

### Binary Operators
When binary operators cannot fit in the same line, split operands after the
binary operators, and align operands with the first operand. Try to use
parentheses as much as possible even if the correct precedence can be derived
without using parentheses. This often helps readers to understand the code
better.

```cpp
if (condition1 ||
   (condition2 && condition3)) {
    ...
}
```

Write the conditions among binary operators with explicit expressions. For details, please refer to [Code readability](#Code-readability)

## Classes
### Constructors
Initialization of the member variables should be done in the initializer as much as possible.
When initializing member variables in a constructor, always split each
initializer on a separate line, and align the commas with the colon.

```cpp
Dog::Dog(String name, Breed breed)
    : Animal()
    , m_name(name)
    , m_breed(breed)
{
...
}
```

Calling other constructors inside a constructor should be avoided.
This is in preparation for an environment where we cannot use c++11 features (such as embedded devices).
```cpp
Dog::Dog()
    , m_isAnimal(true)
{
}

Dog::Dog(String name, Breed breed)
    : Dog()  //<--- Not allowed
    , m_name(name)
    , m_breed(breed)
{
...
}

```

Write it one by one.
```cpp
Dog::Dog()
    , m_isAnimal(true)
    , m_name(String::emptyString)
    , m_breed(Breed::emptyBreed)
{
}

Dog::Dog(String name, Breed breed)
    : m_isAnimal(true)
    , m_name(name)
    , m_breed(breed)
{
...
}

```


### Class Modifiers
Do not indent class modifiers (i.e., `public`, `protected`, and `private`) in
a class declaration.

```cpp
class Dog {
public:
    ...
private:
    ...
}
```

## Functions
### Function Names
Use camelcases when naming a function.

### Function Calls
Write a function call all in the same line if it fits. If not, split the
function call into multiple lines by either adding a newline after the
assignment operator or between function parameters. When splitting between
parameters, align them with the first parameter. In addition, do not add spaces
before and after the first and last function parameter, respectively.

```cpp
int val = foo(arg1, arg2, arg3);

int val =
    foo(arg1, arg2, arg3);

int val = foo(arg1, arg2
              arg3);
```

### Function Declaration and Definition
Use named parameters in function declarations.

```cpp
returnType functionName(int, int); // Not allowed
returnType functionName(int arg1, int arg2);  // Use named parameters
```

Return type should be on the same line as the function declaration
(and definition) if it fits. If not, split function parameters into multiple
lines, and align them with the first parameter.

```cpp
returnType functionName(int arg1,
                        int arg2);
```

If the first parameter does not fit in a line, write parameters in the next
line with 4 space indent.

```cpp
returnType functionName(
    int arg1, int arg2);
```

The opening curly brace should be on its own in the next line.
A Closing curly brace should be on the next line as the opening brace.

```cpp
returnType functionName()
{
    ...
}
```

### Short and Empty Functions
Do not write a function all in one line even for a very short function that
can be fit in one line. One exception is an empty, inline function that is
declared in a header file.

```cpp
void f() { } // Allowed only in a header file

int g()
{
    return 0;
}
```

## C++ Features
### Run-Time Type Information (RTTI)
Do not use Run Time Type Information.

### C++11
Use of C++11 features are encouraged. In addition, use of C++11 compatible
style formatting is encouraged, e.g., use `A<B<int>>` instead of `A<B<int> >`

### Use of `try-catch` statements
Do not use try-catch statements except throwing an Exception.
## Assertions and nullptr
### Basic principle
* When using a pointer type variable, be sure to add the Assertions statement if you do not want to consider the situation where the value is nullptr, if you do not want to add assertions, be sure to write your defense code.
* In our strategy, Escargot will be terminated along with an error message when a memory allocation attempt fails.
* When not using GC allocators, write an assertion after memory allocation.
### Add an assertion in the following situations.
* If the function argument is a pointer type
```cpp
void A::functionA(B* arg1, int arg2) {
    ASSERT(arg1 != nullptr);
}
```
### Handling a nullable pointer

There are the following choices where you handle a pointer which can be nullable.

* Use NULLABLE macro

Add `NULLABLE` definition before the type of pointers. `NULLABLE` is nothing but a notation which explicitly shows whether or not the given pointer can be nullable.

```cpp
// `NULLABLE` is predefined in Escargot as below.
#define NULLABLE

...

NULLABLE Object* A::functionA(NULLABLE ObjectB* b, ObjectC* c) {

    // NOTE: If NULLABLE isn't prepended on the type of the given parameter,
    // ASSERTION is preferred as below.
    ASSERT(c != nullptr);

    ...

    // Access violation should be considered for a nullable pointer.
    if ((b != nullptr) && b->isLoaded())
    {
        ...
    }

    // Use `NULLABLE` when calling a function which can return a nullable pointer.
    NULLABLE Object* obj = c->getObject(...);
    // NULLABLE auto obj = c->getObject(...);

    if (cnd) {
        return new Object;
    } else {
        return nullptr;
    }
}
```

* Use Nullable template only for javascript binding interfaces
```cpp
Nullable<Object> A::functionA() {
    if (cnd) {
        return valueOfObjectClass;
    } else {
        // return nullptr;
        return Nullable<Object>();
    }
}
```

* Use reference instead of pointer

### Be sure to explicitly assign nullptr if you need to release it.
```cpp
A::releaseMemeber() {
    // free(m_pointerMemeber); if you needs
    m_pointerMemeber = nullptr;
}
```
### Supported assertions macro list in Escargot
```cpp
...
ASSERT()
ASSERT_NOT_REACHED()
RELEASE_ASSERT_SHOULD_NOT_BE_HERE()
// ETC, See StarfishBase.h for more information
...
```

## Comments
### Comment Style
Both `//` and `/* */` style comments can be used, although `//` style is
much preferred.
### Add comment for newly function
* Having a clear function name and parameters will solve many readability problems.
* We are not aiming to generate an API doc. We think it is unneeded.
* We don't need to write comment for every function. We prefer to write comments at a place where function definition in cpp file
* What we want to write are (unusual, important, or pre/post conditions of) function behaviors that are difficult to deliver to readers by code. Some examples include, "This function should be called after finishing xxx, or it will give you yyy."
* Since writing comments is optional, we do not want to have rigid formats. (Also, not updating comments after updating actual code is bad). We are thinking of having simple comments starting with `//` in the header file above the function we want to add comments. (Again this is more like informal comment rule.)
* Which function to write the comment is more like up to developers. Each developer needs to decide what to write comments (or not)


## Code readability

Make sure your code is obvious and readable with the following conventions.

```diff
++ // NOTE: the statements in green are preferred.

// Use a more explicit expression for `nullptr` checking.
- if (ptr)
+ if (ptr != nullptr)

- ASSERT(ptr)
+ ASSERT(ptr != nullptr)

// Using the obvious meaning is preferred. ((e.g) the following usage for `strlen`)
- if (verbose && strlen(verbose))
+ if ((verbose != nullptr) && (strlen(verbose) > 0))

// Avoiding conditions with logical operators such as (`!`) is preferred.
// By just using `(!any) or (any)` it's not explicit what you want.
// Rely on the condition itself than variable names like `ptr`.
- if (!any) // The counter of `any` could be read as `nullptr`, `false`, etc.
+ if (any == nullptr)

- if (!o.isLoaded() && ptr == nullptr)
+ if ((o.isLoaded() == false) && ptr == nullptr)
```

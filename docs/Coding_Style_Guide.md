# Escargot Coding Style Guide
The goal of this document is to describe our C++ coding style convention. Our coding style generally follows [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). This document highlights core coding style. 

## Header Files
### `#define` Guard
Use `#define` directive in all header files to prevent multiple inclusion of header files. The format of the identifier name should be `[HeaderFileName]_h`. For example, for `MyHeader.h`, the guard should be `MyHeader_h` .

```cpp
#ifndef MyHeader_h
#define MyHeader_h
...
#endif
```

Don't use identifiers which start with double underscore ('__'). It is reserved for implementation by c++ specification.
See http://stackoverflow.com/questions/10077025/ifndef-syntax-for-include-guards-in-c


#ifndef __ESCARGOT_PUBLIC__
#define __ESCARGOT_PUBLIC__

#include <cstdlib> // size_t

namespace Escargot {

class Globals {
    static void initialize();
};

class VMInstanceRef {
    explicit VMInstanceRef();
};

class ContextRef {
    explicit ContextRef(VMInstanceRef vminstance);
};

class ExecutionStateRef {
    explicit ExecutionStateRef(ContextRef vminstance);
};

class ASCIIStringRef {
    ASCIIStringRef(const char* str);
    ASCIIStringRef(const char* str, size_t len);
    ASCIIStringRef(const char16_t* str, size_t len);
};

} // namespace Escargot
#endif

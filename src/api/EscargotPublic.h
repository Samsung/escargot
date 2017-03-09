#ifndef __ESCARGOT_PUBLIC__
#define __ESCARGOT_PUBLIC__

#include <cstdlib> // size_t

namespace Escargot {

class StringRef {
public:
    static StringRef* fromASCII(const char* s);
};

class Globals {
public:
    static void initialize();
    static void finalize();
};

class VMInstanceRef {
public:
    static VMInstanceRef* create();
};

class ContextRef {
public:
    static ContextRef* create(VMInstanceRef* vminstance);
};

class ExecutionStateRef {
public:
    static ExecutionStateRef* create(ContextRef* ctx);
};

class ASCIIStringRef {
public:
    static ASCIIStringRef* create(const char* str);
    static ASCIIStringRef* create(const char* str, size_t len);
    static ASCIIStringRef* create(const char16_t* str, size_t len);
};

bool evaluateScript(ContextRef* ctx, StringRef* script, StringRef* fileName);

} // namespace Escargot
#endif

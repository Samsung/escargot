#ifndef __EscargotHeap__
#define __EscargotHeap__
#include "GCUtil.h"

namespace Escargot {

class Heap {
public:
    static void initialize();
    static void finalize();
};
}

#include "CustomAllocator.h"

#endif

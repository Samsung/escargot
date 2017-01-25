#ifndef __EscargotUtil__
#define __EscargotUtil__

namespace Escargot {

inline void __attribute__((optimize("O0"))) fillStack(size_t siz)
{
    volatile char a[siz];
    memset((char*)a, 0, siz);
}

}
#endif

#ifndef __EscargotUtil__
#define __EscargotUtil__

namespace Escargot {

template <const int siz>
inline void __attribute__((optimize("O0"))) clearStack()
{
    volatile char a[siz] = { 0 };
}
}
#endif

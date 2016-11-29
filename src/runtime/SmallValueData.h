#ifndef __EscargotSmallValueData__
#define __EscargotSmallValueData__

namespace Escargot {

union SmallValueData {
    intptr_t payload;
    SmallValueData()
    {

    }

    SmallValueData(void* ptr)
    {
        payload = (intptr_t)ptr;
    }
};

}


#endif

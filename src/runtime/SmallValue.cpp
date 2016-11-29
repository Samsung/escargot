#include "Escargot.h"
#include "SmallValueData.h"
#include "PointerValue.h"

namespace Escargot {

class DummyInSmallValue : public PointerValue {
public:
    virtual Type type()
    {
        return DummyInSmallValueType;
    }

    virtual bool isDummyInSmallValueType()
    {
        return true;
    }
};

namespace SmallValueImpl {

static DummyInSmallValue dummyInSmallValueUndefined;
static DummyInSmallValue dummyInSmallValueNull;
static DummyInSmallValue dummyInSmallValueTrue;
static DummyInSmallValue dummyInSmallValueFalse;
static DummyInSmallValue dummyInSmallValueEmpty;
static DummyInSmallValue dummyInSmallValueDelete;

SmallValueData* smallValueUndefined = new SmallValueData(&dummyInSmallValueUndefined);
SmallValueData* smallValueNull = new SmallValueData(&dummyInSmallValueNull);
SmallValueData* smallValueTrue = new SmallValueData(&dummyInSmallValueTrue);
SmallValueData* smallValueFalse = new SmallValueData(&dummyInSmallValueFalse);
SmallValueData* smallValueEmpty = new SmallValueData(&dummyInSmallValueEmpty);
SmallValueData* smallValueDeleted = new SmallValueData(&dummyInSmallValueDelete);
}

}

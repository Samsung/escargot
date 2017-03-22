# Public APIs

## The Name of public API header 

1) Escargot.h

We need to rename former Escargot.h to EscargotDef.h. Current Escargot.h is not the api header. It is collection of macros like LIKELY, COMPILER.

2) EscargotAPI.h

JerryScript uses the name api, exactly jerry-api.h

3) EscargotPublic.h

Starfish uses this convention.

## The Name of global utilities
Like `v8::V8::Initialize`, we sometimes want to initialize or get or set something belongs into global.
Here, by `global', I means it does not belong to a specific VMInstance.
For example, ICU or Heap is initialized and used by all VMInstances. I want to put them in a class.
In v8, it uses V8. Please notify v is capitalized. What would be suitable to us ?

1) ~~Escargot::Escargot2~~

2)  Escargot::Globals

3)  Escargot::Public

# Isolate or Not Isolate
We don't support isolation. We share many things including heap.We use globally unique JS heap space backing by boehm gc.
So Escargot had not needed a pointer to JS heap. Besides, in typical JavaScript Engines, all the function that may allocate memory from JavaScript heap must provides a way to access to JS heap. Should we prepare the situation of isolate support? If so, we need to design almost all APis accepts VMInstace as the first parameter.

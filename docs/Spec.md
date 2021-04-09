# Specification

Escargot is a lightweight JavaScript engine.  
Escargot supports [ECMAScript 2020 Specification](https://262.ecma-international.org/11.0/) and [WebAssembly JavaScript Interface](https://www.w3.org/TR/wasm-js-api-1/).


## ECMAScript 2020 Specification
Implement ECMAScript 2020 standard (https://262.ecma-international.org/11.0/).

#### Unimplemented Features
* [Atomics Object](https://tc39.es/ecma262/#sec-atomics-object)
* [SharedArrayBuffer Object](https://tc39.es/ecma262/#sec-sharedarraybuffer-objects)
* [look-behind assertions](https://tc39.es/ecma262/#sec-assertion)

#### Escargot-Specific Features
Even though the features below are not part of ECMAScript standards, Escargot supports them for usability.  
(These features are enabled only for standalone binary build)

| Feature | Function |
| ------- | -------- |
| print (arg) | prints `arg` on stdout |
| load (fileName) | opens a file named `fileName` and executes the contents as source code |
| read (fileName) | opens a file named `fileName` and returns the contents as string |
| run (fileName) | opens a file named `fileName`, executes the contents as source code, and returns the time spent to execute the code |
| gc() | invokes garbage collector |


## WebAssembly JavaScript Interface
Implement WebAssembly JavaScript Interface (https://www.w3.org/TR/wasm-js-api-1/).

#### WebAssembly Core
Import [WABT](https://github.com/WebAssembly/wabt) for WebAssembly core engine.
WABT supports [WebAssembly Core Specification](https://www.w3.org/TR/wasm-core-1/).

#### Unimplemented Features
* [Module.customSections](https://www.w3.org/TR/wasm-js-api-1/#dom-module-customsections)


## Code Caching Optimization
Support Code Caching method to accelerate the loading performance.  
Code Caching stores the bytecode for the first time and reuses it when the same JavaScript code is executed again.  
By reusing the bytecode, Code Caching could skip the parsing and compilation process which leads to fast loading time.

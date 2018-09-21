# Specification

Escargot is lightweight JavaScript engine used in Pando.
Escargot implements full of [ECMAScript 5.1 specification](http://www.ecma-international.org/ecma-262/5.1/) and part of [ECMAScript 6.0 specification](http://www.ecma-international.org/ecma-262/6.0/).

## ES5
ES5 specification is fully supported with Escargot.

## ES6

### supported
| Object type | Implemented Feature | Property Type | Note |
| -------- | ---- | ------------------- | ---- |
| [TypedArray Objects](http://www.ecma-international.org/ecma-262/6.0/#sec-typedarray-objects) | TypedArray () | constructor | Currently it works expectedly only with arraylength less than 210000000 |
| | TypedArray (length) | constructor | |
| | TypedArray (buffer [, byteOffset [, length]]) | constructor | |
| | TypedArray.prototype.indexOf (searchElement [, fromIndex]) | method | |
| | TypedArray.prototype.lastIndexOf (searchElement [, fromIndex]) | method | |
| | TypedArray.prototype.set (array [, offset]) | method | |
| | TypedArray.prototype.set (typedArray [, offset]) | method | |
| | TypedArray.prototype.subarray ([begin [, end]]) | method | |
| | TypedArray.BYTES_PER_ELEMENT | data property | |
| | TypedArray.prototype.BYTES_PER_ELEMENT | data property | |
| | TypedArray.prototype.constructor | data property | |
| | getter of TypedArray.prototype.byteLength | accessor property | |
| | getter of TypedArray.prototype.byteOffset | accessor property | |
| | getter of TypedArray.prototype.length | accessor property | |
| | getter of TypedArray.prototype.buffer | accessor property | |
| [ArrayBuffer Objects](http://www.ecma-international.org/ecma-262/6.0/#sec-arraybuffer-objects) | ArrayBuffer (length) | constructor | |
| | ArrayBuffer.isView (arg) | function | |
| | getter of ArrayBuffer.prototype.byteLength | accessor property | |
| | ArrayBuffer.prototype.constructor | data property | |
| | ArrayBuffer.prototype.slice (start, end) | method | |
| [DataView Objects](http://www.ecma-international.org/ecma-262/6.0/#sec-dataview-objects) | DataView (buffer [, byteOffset [, byteLength]]) | constructor | |
| | getter of DataView.prototype.buffer | accessor property | |
| | getter of DataView.prototype.byteLength | accessor property | |
| | getter of DataView.prototype.byteOffset | accessor property | |
| | DataView.prototype.constructor | data property | |
| | DataView.prototype.[get &#124; set][Float32 &#124; Float64 &#124; Int8 &#124; Int16 &#124; Int32 &#124; Uint8 &#124; Uint16 &#124; Uint32] \(byteOffset [, littleEndian)) | method | |
| [Promise Objects](http://www.ecma-international.org/ecma-262/6.0/#sec-promise-objects) | Promise (executor) | constructor | |
| | Promise.all (iterable) | function | |
| | Promise.race (iterable) | function | |
| | Promise.reject (r) | function | |
| | Promise.resolve (x) | function | |
| | Promise.prototype.catch (onRejected) | method | |
| | Promise.prototype.then (onFulfilled, onRejected) | method | |


## Etc.
Even though the features below are not included in ES5 or ES6 specification, Escargot supports them for your usability.

### Legacy features
| Implemented Feature | Note |
| ------------------- | ---- |
| Object.prototype.\_\_proto\_\_ | |
| Object.prototype.\_\_defineGetter\_\_ (prop, func) | |
| Object.prototype.\_\_defineSetter\_\_ (prop, func) | |
| Object.prototype.\_\_lookupGetter\_\_ (prop) | |
| Object.prototype.\_\_lookupSetter\_\_ (prop) | |
| escape (str) | |
| unescape (str) | |

### Escargot-specific features

| Implemented Feature | Function | Note |
| ------------------- | -------- | ---- |
| print (arg) | prints `arg` on stdout | standalone binary only |
| load (fileName) | opens a file named `fileName` and executes the contents as source code | standalone binary only |
| read (fileName) | opens a file named `fileName` and returns the contents as string | standalone binary only |
| run (fileName) | opens a file named `fileName`, executes the contents as source code, and returns the time spent to execute the code | standalone binary only |
| gc() | invokes garbage collector | |

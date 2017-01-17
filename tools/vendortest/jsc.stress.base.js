var origLoad = load;
load = function(path) {
    var reloadWith = function(prefix, path) {
        try {
            return origLoad(prefix + path);
        } catch (e) {
            return e;
        }
    }

    var ret;
    try {
        return origLoad(path);
    } catch (e) {
        ret = reloadWith("test/vendortest/JavaScriptCore/stress/", path);
    }

    if (ret instanceof Error)
        throw ret;
}

function edenGC() {
    gc();
}

function fullGC() {
    gc();
}

function DFGTrue() {
    return false;
}

function debug(msg) {
    print(msg);
}

function numberOfDFGCompiles() {}
function neverInlineFunction() {}

var self = this;

self.testRunner = {
    neverInlineFunction: neverInlineFunction,
    numberOfDFGCompiles: numberOfDFGCompiles
};

function noInline(theFunction)
{
    if (!self.testRunner)
        return;

    testRunner.neverInlineFunction(theFunction);
}

function isInt32(e) {
    return false; // no error
    // return true; // Uncaught Error: bad result: true
}


function loadWebAssembly(url) {
    load(url); // should load wsam file here
}


function OSRExit() {

}

function effectful42() {
    return 42;
}

function fiatInt52(arg) {
    return arg;
}

function isFinalTier() {
    return false; // escargot do not use jit or reoptimization
}

function reoptimizationRetryCount() {
    return 0; // escargot do not use jit or reoptimization
}

function noDFG() {
    // escargot do not use jit or reoptimization
}

function predictInt32() {
    // escargot do not use jit or reoptimization
}

function createProxy(arg) {
    return arg;
}

Object.defineProperty(Object.prototype, "__defineGetter__", {
    value : function(prop, func) { Object.defineProperty(this, prop, {get: func, enumerable : true, configurable : true}); },
    enumerable : false
});

Object.defineProperty(Object.prototype, "__defineSetter__", {
    value : function(prop, func) { Object.defineProperty(this, prop, {set: func, enumerable : true, configurable : true}); },
    enumerable : false
});

Object.is = function(a,b) {
    if (a === b)
        return true;
    if (isNaN(a) && isNaN(b))
        return true;
    return false;
}

Object.assign = function(src, target) {
    for (var it in target) {
        if (Object.hasOwnProperty(target, it))
            src[it] = target[it];
    }
    return src;
}

Object.setPrototypeOf = function(obj, proto) {
    // from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/setPrototypeOf#Polyfill
    if (obj.__proto__ == proto)
      return obj;

    obj.__proto__ = proto;
    return obj;
};

Number.isNaN = function(arg) { return isNaN(arg); };
Math.fround = function(arg) { return Math.round(arg); };

var Reflect = {
    setPrototypeOf : function(obj, proto) {
      try {
        Object.setPrototypeOf(obj, proto);
        return true;
      } catch(e) {
        return false;
      }
    },
    getPrototypeOf : Object.getPrototypeOf
}

/*

function hasCustomProperties(o){
    if(o) return true; // true : Uncaught Error : object should't have custom properties yet
    else return false; // false: Uncaught Error : object should have custom properties already
}

*/

var global = this;
load("polyfill.es6-shim.js");
load("polyfill.symbol.js");

/* add version() to test js1_2 by youri */
function version() {
  return 120;
}

/* add finalizeCount() & makeFinalizeObserver by youri */
/* it doesn't needed probably*/
function finalizeCount() {
  return 0;
}

function makeFinalizeObserver() {
  return 0;
}

function evalcx(str, object) {
    if (object != undefined)
        throw "evalcx() not supported";
    if (str == "lazy" && object == undefined)
        throw "evalcx() not supported";
    if (str == '')
        throw "evalcx() not supported";

    return eval(str);
}

function uneval(value) {
    return "" + value;
}

function jit(arg) {
    // do nothing on escargot
}


Object.defineProperty(Object.prototype, "__defineGetter__", {
    value : function(prop, func) { Object.defineProperty(this, prop, {get: func, enumerable : true, configurable : true}); },
    enumerable : false
});

Object.defineProperty(Object.prototype, "__defineSetter__", {
    value : function(prop, func) { Object.defineProperty(this, prop, {set: func, enumerable : true, configurable : true}); },
    enumerable : false
});


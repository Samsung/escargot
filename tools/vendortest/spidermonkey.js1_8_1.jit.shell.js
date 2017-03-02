// The loop count at which we trace
var RECORDLOOP = this.tracemonkey ? tracemonkey.HOTLOOP : 8;
// The loop count at which we run the trace
var RUNLOOP = RECORDLOOP + 1;

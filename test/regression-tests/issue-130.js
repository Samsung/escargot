var r = / /;

var failures = ['global', 'ignoreCase', 'multiline', 'source'];

failures.forEach(function(prop) {
  try {
    Object.defineProperty(r, prop, {value: false});
    assert(false);
  } catch(e) {
    assert(e instanceof TypeError);
  }
});

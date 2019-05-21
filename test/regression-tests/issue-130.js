var r = / /;

var properties = ['global', 'ignoreCase', 'multiline', 'source'];

properties.forEach(function(prop) {
  try {
    Object.defineProperty(r, prop, {value: true});
    assert(r[prop]);
    Object.defineProperty(r, prop, {value: false});
    assert(false);
  } catch(e) {
    assert(r[prop]);
    assert(e instanceof TypeError);
  }
});

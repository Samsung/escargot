eval("function f() {\nreturn 5\n}")
f()

function g(x) {
  return x
}

eval("var x = 0\ng()\ng()")

function h() {
  var g = 1
  eval("g = 2\ng = 3")
  global_eval = eval
  global_eval("x=1\n\ng()")
}

h()

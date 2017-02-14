# Escargot

## Building

``` sh
git clone git@10.113.64.74:webtf/escargot2.git
cd escargot2
./build_third_party.sh
make [x86|x64].interpreter.[debug|release] -j
```

e.g. `make x64.interpreter.debug -j`, `make x64.interpreter.release -j`

To get available configuration list, get help of completion

``` sh
make <tab><tab>
```

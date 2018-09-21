# Escargot

## Prerequisites
```
apt-get install autoconf automake libtool libc++-dev libicu-dev gcc-multilib g++-multilib
```

## Building

``` sh
git clone git@github.com:pando-project/escargot.git
cd escargot
git submodule init
git submodule update third_party/GCutil
./build_third_party.sh
make [x86|x64].interpreter.[debug|release] -j
```

e.g. `make x64.interpreter.debug -j`, `make x64.interpreter.release -j`

To get available configuration list, get help of completion

``` sh
make <tab><tab>
```

## Testing

First, get benchmarks and tests


``` sh
git submodule init
git submodule update
```

### Sunspider

```sh
make run-sunspider
```

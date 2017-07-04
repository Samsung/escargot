# Escargot

## Prerequisites
```
apt-get install autoconf automake libtool libc++-dev libicu-dev gcc-multilib g++-multilib
```

## Building

``` sh
git clone git@github.sec.samsung.net:RS7-webtf/escargot.git
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

## Misc.

### CI Infrastructure

http://10.113.64.74:8080/job/escargot2_daily_measure/

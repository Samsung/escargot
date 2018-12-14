# Escargot

[![Travis CI Build Status](https://travis-ci.org/pando-project/escargot.svg?branch=master)](https://travis-ci.org/pando-project/escargot)

## Prerequisites
```
apt-get install autoconf automake libtool libc++-dev libicu-dev gcc-multilib g++-multilib
```

## Build Escargot

``` sh
git clone git@github.com:pando-project/escargot.git
cd escargot
git submodule update --init third_party
cmake CMakeLists.txt -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin
make -j
```

#### Build options

The following build options are supported when generating Makefile using cmake.

* -DESCARGOT_HOST=[ linux | tizen_obs ]<br>
  Compile Escargot for either Linux or Tizen platform
* -DESCARGOT_ARCH=[ x64 | x86 | arm | i686 | aarch64 ]<br>
  Compile Escargot for each architecture
* -DESCARGOT_MODE=[ debug | release ]<br>
  Compile Escargot for either release or debug mode
* -DESCARGOT_OUTPUT=[ bin | shared_lib | static_lib ]<br> 
  Define target output type

## Testing

First, get benchmarks and tests

``` sh
git submodule update --init
```

### Benchmarks

```sh
make run-sunspider-js # Sunspider
make run-octane # Octane
make run-v8-x64 # V8 for x64
make run-chakracore-x64 # Chakracore for x64
make run-test262 # test262
```

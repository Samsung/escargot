# Escargot

[![Travis CI Build Status](https://travis-ci.org/pando-project/escargot.svg?branch=master)](https://travis-ci.org/pando-project/escargot)
[![SonarCloud Status](https://sonarcloud.io/api/project_badges/measure?project=pando-project_escargot&metric=alert_status)](https://sonarcloud.io/dashboard?id=pando-project_escargot)

## Prerequisites

General build prerequisites:
```sh
sudo apt-get install autoconf automake libtool libicu-dev
```

Prerequisites for x86-64-to-x86 compilation:
```sh
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install libicu-dev:i386
```

## Build Escargot

``` sh
git clone git@github.com:pando-project/escargot.git
cd escargot
git submodule update --init third_party
cmake CMakeLists.txt -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin -G Ninja
ninja
```

#### Build options

The following build options are supported when generating ninja rules using cmake.

* -DESCARGOT_HOST=[ linux | tizen_obs ]<br>
  Compile Escargot for either Linux or Tizen platform
* -DESCARGOT_ARCH=[ x64 | x86 | arm | i686 | aarch64 ]<br>
  Compile Escargot for each architecture
* -DESCARGOT_MODE=[ debug | release ]<br>
  Compile Escargot for either release or debug mode
* -DESCARGOT_OUTPUT=[ bin | shared_lib | static_lib ]<br> 
  Define target output type

## Testing

First, get benchmarks and tests:
```sh
git submodule update --init
```

### Benchmarks

Prerequisite for SpiderMonkey:
```sh
sudo apt-get install npm
npm install
```

Test run for each benchmark (Sunspider, Octane, V8, Chakracore, test262,
SpiderMonkey, etc.):
```sh
tools/run-tests.py
```

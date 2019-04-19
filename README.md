# Escargot

[![Travis CI Build Status](https://travis-ci.org/pando-project/escargot.svg?branch=master)](https://travis-ci.org/pando-project/escargot)
[![SonarCloud Status](https://sonarcloud.io/api/project_badges/measure?project=pando-project_escargot&metric=alert_status)](https://sonarcloud.io/dashboard?id=pando-project_escargot)

## Prerequisites

#### On Ubuntu Linux

General build prerequisites:
```sh
sudo apt-get install autoconf automake libtool libicu-dev ninja-build
```

Prerequisites for x86-64-to-x86 compilation:
```sh
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install libicu-dev:i386
```

#### On macOS

```sh
brew install autoconf automake cmake icu4c libtool ninja pkg-config
```

Note: For later build steps (cmake, pkg-config) to find ICU libraries, you may
need to set the `PKG_CONFIG_PATH` environment variable, as instructed by brew.
E.g.:

```sh
export PKG_CONFIG_PATH="/usr/local/opt/icu4c/lib/pkgconfig"
```

## Build Escargot

```sh
git clone https://github.com/pando-project/escargot.git
cd escargot
git submodule update --init third_party
cmake -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin -GNinja
ninja
```

#### Build options

The following build options are supported when generating ninja rules using cmake.

* -DESCARGOT_HOST=[ linux | tizen_obs | darwin ]<br>
  Compile Escargot for Linux, Tizen, or macOS platform
* -DESCARGOT_ARCH=[ x64 | x86 | arm | i686 | aarch64 ]<br>
  Compile Escargot for each architecture
* -DESCARGOT_MODE=[ debug | release ]<br>
  Compile Escargot for either release or debug mode
* -DESCARGOT_OUTPUT=[ bin | shared_lib | static_lib ]<br>
  Define target output type
* -DESCARGOT_LIBICU_SUPPORT=[ ON | OFF ]<br>
  Enable libicu library if set ON. (Optional, default = ON)

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

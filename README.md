# Return-Check Version of Escargot

## Prerequisites

#### On Ubuntu Linux

General build prerequisites:
```sh
sudo apt-get install autoconf automake cmake libtool libicu-dev ninja-build
```

Prerequisites for x86-64-to-x86 compilation:
```sh
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install libicu-dev:i386
```

## Build Escargot

build core runtime of Escargot engine (excluding intl, temporal etc)
```sh
git clone https://github.com/Samsung/escargot.git -b return-check
cd escargot
git submodule update --init third_party
cmake -H. -Bout/test/release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=[x86|x64|arm|aarch64] -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell -DESCARGOT_THREADING=ON -DESCARGOT_TEST=ON -GNinja
ninja -Cout/test/release
```

## Testing

#### octane

Using test runner
```sh
tools/run-tests.py --engine=ESCARGOT_BINARY_PATH octane
```

Or directly run octane benchmarks
```sh
cd test/octane/
./escargot run.js
```
#### sunspider

Using test runner
```sh
tools/run-tests.py --engine=ESCARGOT_BINARY_PATH sunspider-js
```

#### kraken

download kraken benchmark
```sh
git clone https://github.com/mozilla-it/krakenbenchmark.mozilla.org.git
```
run kraken benchmark
```sh
cd krakenbenchmark.mozilla.org
./sunspider --shell ESCARGOT_BINARY_PATH --suite kraken-1.1
```
results would be stored in `kraken-1.1-results` directory

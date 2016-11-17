#!/bin/bash

if [ -z "$ANDROID_NDK_STAND_ALONE" ]; then
    echo "Need to set ANDROID_NDK_STAND_ALONE"
    exit 1
fi

echo "ANDROID_NDK_STAND_ALONE env is ..."
echo $ANDROID_NDK_STAND_ALONE

cd ..
autoreconf -vif
automake --add-missing
cd android_multithreaded

make distclean

export CC="arm-linux-androideabi-gcc"
export LD="arm-linux-androideabi-ld"
export PATH=$ANDROID_NDK_STAND_ALONE/bin:$PATH

../configure --enable-threads=posix --disable-parallel-mark --disable-gc-debug --build=arm-linux-androideabi --target=arm-linux-androideabi --with-sysroot=$ANDROID_NDK_STAND_ALONE --host=x86_64-unknown-linux-gnu CFLAGS=''
make
